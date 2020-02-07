/*
* Copyright 2013 Sveriges Television AB http://casparcg.com/
*
* This file is part of CasparCG (www.casparcg.com).
*
* CasparCG is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* CasparCG is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with CasparCG. If not, see <http://www.gnu.org/licenses/>.
*
* Author: Robert Nagy, ronag89@gmail.com
*/

#include "html.h"

#include "producer/html_producer.h"

#include <common/concurrency/executor.h>
#include <common/concurrency/future_util.h>

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/foreach.hpp>
#include <boost/timer.hpp>
#include <boost/range/algorithm/remove_if.hpp>
#include <boost/thread/future.hpp>
#include <boost/lexical_cast.hpp>

#include <cef_app.h>

#pragma comment(lib, "libcef.lib")
#pragma comment(lib, "libcef_dll_wrapper.lib")

namespace caspar { namespace html {

std::unique_ptr<executor> g_cef_executor;

void caspar_log(
	const CefRefPtr<CefBrowser>& browser,
	boost::log::trivial::severity_level level,
	const std::string& message)
{
	if (browser)
	{
		auto msg = CefProcessMessage::Create(LOG_MESSAGE_NAME);
		msg->GetArgumentList()->SetInt(0, level);
		msg->GetArgumentList()->SetString(1, message);
		browser->SendProcessMessage(PID_BROWSER, msg);
	}
}

class remove_handler : public CefV8Handler
{
	CefRefPtr<CefBrowser> browser_;
public:
	remove_handler(CefRefPtr<CefBrowser> browser)
		: browser_(browser)
	{
	}

	bool Execute(
		const CefString& name,
		CefRefPtr<CefV8Value> object,
		const CefV8ValueList& arguments,
		CefRefPtr<CefV8Value>& retval,
		CefString& exception) override
	{
		if (!CefCurrentlyOn(TID_RENDERER))
			return false;

		browser_->SendProcessMessage(
			PID_BROWSER,
			CefProcessMessage::Create(REMOVE_MESSAGE_NAME));

		return true;
	}

	IMPLEMENT_REFCOUNTING(remove_handler);
};

class browser_application : public CefApp
{
public:
	void OnBeforeCommandLineProcessing(
		const CefString& process_type,
		CefRefPtr<CefCommandLine> command_line) override {
		if (!process_type.empty())
			return;

		command_line->AppendSwitch("disable-web-security");
// STL 20190228 pour pouvoir jouer des sites https avec des certificats autosognés
		command_line->AppendSwitch("ignore-certificate-errors");
		command_line->AppendSwitch("enable-media-stream");
		command_line->AppendSwitchWithValue("js-flags", "--expose-gc");
		// STL pour désactiver le son des pages html, je pense qu'il est possible d'utiliser l'option 
		//command_line->AppendSwitch("mute-audio");


		// Possible required flags.
		command_line->AppendSwitch("disable-gpu");
		command_line->AppendSwitch("disable-gpu-compositing");
		command_line->AppendSwitch("enable-begin-frame-scheduling");
	}

private:
	IMPLEMENT_REFCOUNTING(browser_application);
};

class renderer_application : public CefApp, CefRenderProcessHandler
{
public:
	CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override
	{
		return this;
	}

	void OnContextCreated(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefRefPtr<CefV8Context> context) override
	{
		caspar_log(browser, boost::log::trivial::trace,
			"context for frame "
			+ boost::lexical_cast<std::string>(frame->GetIdentifier())
			+ " created");

		auto window = context->GetGlobal();
		
		auto function = CefV8Value::CreateFunction(
			"remove",
			new remove_handler(browser));
		window->SetValue(
			"remove",
			function,
			V8_PROPERTY_ATTRIBUTE_NONE);
	}
	
	IMPLEMENT_REFCOUNTING(renderer_application);
};

bool init()
{
	CefMainArgs main_args;

	if (CefExecuteProcess(main_args, CefRefPtr<CefApp>(new renderer_application), nullptr) >= 0)
		return false;
	SetPriorityClass(main_args.instance, HIGH_PRIORITY_CLASS);
	core::register_producer_factory(html::create_producer);

	g_cef_executor.reset(new executor(L"cef"));
	g_cef_executor->invoke([&]
	{
		CefSettings settings;
		CefInitialize(main_args, settings, new browser_application, nullptr);
	});
	g_cef_executor->begin_invoke([&]
	{
		CefRunMessageLoop();
	});

	return true;
}

void uninit()
{
	invoke([]
	{
		CefQuitMessageLoop();
	});
	g_cef_executor->invoke([&]
	{
		CefShutdown();
	});
	g_cef_executor.reset();
}

class cef_task : public CefTask
{
private:
	boost::promise<void> promise_;
	std::function<void ()> function_;
public:
	cef_task(const std::function<void ()>& function)
		: function_(function)
	{
	}

	void Execute() override
	{
		CASPAR_LOG(trace) << "[cef_task] executing task";

		try
		{
			function_();
			promise_.set_value();
			CASPAR_LOG(trace) << "[cef_task] task succeeded";
		}
		catch (...)
		{
			promise_.set_exception(boost::current_exception());
			CASPAR_LOG(warning) << "[cef_task] task failed";
		}
	}

	boost::unique_future<void> future()
	{
		return promise_.get_future();
	}

	IMPLEMENT_REFCOUNTING(cef_task);
};

void invoke(const std::function<void()>& func)
{
	begin_invoke(func).get();
}


boost::unique_future<void> begin_invoke(const std::function<void()>& func)
{
	CefRefPtr<cef_task> task = new cef_task(func);

	if (CefCurrentlyOn(TID_UI))
	{
		// Avoid deadlock.
		task->Execute();
		return task->future();
	}

	if (CefPostTask(TID_UI, task.get()))
		return task->future();
	else
		BOOST_THROW_EXCEPTION(caspar_exception()
			<< msg_info("[cef_executor] Could not post task"));
}

}}
