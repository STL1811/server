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

#include "../StdAfx.h"
 
#include "SharedMem_consumer.h"

#include "../util/util.h"
//#include "../util/decklink_allocator.h"
#include "../SharedMem.h"

//#include "../interop/DeckLinkAPI_h.h"

#include <core/mixer/read_frame.h>

#include <common/concurrency/com_context.h>
#include <common/concurrency/future_util.h>
#include <common/diagnostics/graph.h>
#include <common/exception/exceptions.h>
#include <common/exception/win32_exception.h>
#include <common/utility/assert.h>
#include <common/utility/software_version.h>
#include <common/memory/endian.h>

#include <core/parameters/parameters.h>
#include <core/consumer/frame_consumer.h>
#include <core/mixer/audio/audio_util.h>

#include <tbb/concurrent_queue.h>
#include <tbb/cache_aligned_allocator.h>

#include <boost/circular_buffer.hpp>
#include <boost/timer.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/algorithm/string.hpp>

namespace caspar { namespace SharedMem { 
	


struct SharedMem_consumer : public  boost::noncopyable
{
	/*
	const int										channel_index_;
	const configuration								config_;

	std::unique_ptr<thread_safe_decklink_allocator>	allocator_;
	CComPtr<IDeckLink>								decklink_;
	CComQIPtr<Output>								output_;
	CComQIPtr<IDeckLinkKeyer>						keyer_;
	CComQIPtr<IDeckLinkAttributes>					attributes_;
	CComQIPtr<IDeckLinkConfiguration>				configuration_;

	tbb::spin_mutex									exception_mutex_;
	std::exception_ptr								exception_;

	tbb::atomic<bool>								is_running_;
		
	const std::wstring								model_name_;
	const core::video_format_desc					format_desc_;
	const size_t									buffer_size_;

	long long										video_scheduled_;
	long long										audio_scheduled_;

	size_t											preroll_count_;
		
	boost::circular_buffer<std::vector<int32_t, tbb::cache_aligned_allocator<int32_t>>>	audio_container_;

	tbb::concurrent_bounded_queue<std::shared_ptr<core::read_frame>> video_frame_buffer_;
	tbb::concurrent_bounded_queue<std::shared_ptr<core::read_frame>> audio_frame_buffer_;
	
	safe_ptr<diagnostics::graph> graph_;
	boost::timer tick_timer_;
	retry_task<bool> send_completion_;
	reference_signal_detector<Output> reference_signal_detector_;

	tbb::atomic<int64_t>						current_presentation_delay_;
	tbb::atomic<int64_t>						scheduled_frames_completed_;
	std::unique_ptr<key_video_context<Output>>	key_context_;
	*/
	core::video_format_desc format_desc_;
	int						channel_index_;
	core::channel_layout audio_layout_;
			
	float					width_;
	float					height_;	
	unsigned int			screen_x_;
	unsigned int			screen_y_;
	unsigned int			screen_width_;
	unsigned int			screen_height_;
	size_t					square_width_;
	size_t					square_height_;				
	unsigned int mCurrentSample;

	std::int64_t			pts_;
	
	safe_ptr<diagnostics::graph>	graph_;
	boost::timer					frame_timer_;
	boost::timer					tick_timer_;

	
	tbb::concurrent_bounded_queue<safe_ptr<core::read_frame>>	frame_buffer_;

	boost::thread			thread_;
	tbb::atomic<bool>		is_running_;
	tbb::atomic<int64_t>	current_presentation_age_;
	CasparSHM_t *shm[CIRCULAR_BUFFER_SIZE];
	CasparSHM_t *shmAudio[CIRCULAR_BUFFER_SIZE];
	executor						executor_;

	const configuration								config_;
	HANDLE hMapFile[CIRCULAR_BUFFER_SIZE];
	HANDLE hAudioMapFile[CIRCULAR_BUFFER_SIZE];
	int buffer_index_;

public:
	SharedMem_consumer(
			const configuration& config,
			const core::video_format_desc& format_desc,
			int channel_index) 
			: executor_(print())
		, config_(config)
		, format_desc_(format_desc)
		, channel_index_(channel_index)
		, mCurrentSample(0)
	{
		graph_->set_color("tick-time", diagnostics::color(0.0f, 0.6f, 0.9f));	
		graph_->set_color("frame-time", diagnostics::color(0.1f, 1.0f, 0.1f));
		graph_->set_color("dropped-frame", diagnostics::color(0.3f, 0.6f, 0.3f));
//		audio_layout_ = core::default_channel_layout_repository().get_by_name(L"STEREO");
		audio_layout_ = core::create_custom_channel_layout(
			L"PASSTHRU",
			core::default_channel_layout_repository());
		graph_->set_text(print());
		diagnostics::register_graph(graph_);

		is_running_ = true;
		current_presentation_age_ = 0;
	
		std::wstring shm_path;
		std::wstring shm_audio_path;
				DWORD dwLastError; 

		if (config_.as_admin == true)
		{
			shm_path = L"Global\\LORAVideoBufferConsumer"+ boost::lexical_cast<std::wstring>(config_.device_index);	
			shm_audio_path = L"Global\\LORAAudioBufferConsumer"		   + boost::lexical_cast<std::wstring>(config_.device_index);	
		}
		else
		{
			shm_path = L"Local\\LORAVideoBufferConsumer"+ boost::lexical_cast<std::wstring>(config_.device_index);	
			shm_audio_path = L"Local\\LORAAudioBufferConsumer"		   + boost::lexical_cast<std::wstring>(config_.device_index);	
		}

/*		} else {
			shm_path = L"Local\\CasparSHM" + boost::lexical_cast<std::wstring>(device_index_);		   
		}*/
		for(int i =0; i<CIRCULAR_BUFFER_SIZE; i++)
		{
			auto current_shm_path = shm_path + L"_" + boost::lexical_cast<std::wstring>(i);	
			auto current_shm_audio_path = shm_audio_path + L"_" + boost::lexical_cast<std::wstring>(i);	
			hMapFile[i] = ::CreateFileMapping(
                		 INVALID_HANDLE_VALUE,    // use paging file
            			 NULL,                    // default security
               			 PAGE_READWRITE,          // read/write access
               			 0,                       // maximum object size (high-order DWORD)
               			 BUF_SIZE,                // maximum object size (low-order DWORD)
						 reinterpret_cast <LPCWSTR> (current_shm_path.c_str()));                 // name of mapping object
			dwLastError = ::GetLastError();
			CASPAR_LOG(trace) << print() << L"CreateFileMapping video "<< current_shm_path <<"last error : " <<dwLastError;

			if (hMapFile[i] == NULL) 
			{
				if (GetLastError() == ERROR_ALREADY_EXISTS)
				{
					hMapFile[i]= ::OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, reinterpret_cast <LPCWSTR> (current_shm_path.c_str()));
					if (hMapFile[i] == NULL)
						BOOST_THROW_EXCEPTION(caspar_exception() << msg_info("Could not create file mapping object"));
				}

			}
			shm[i] = (CasparSHM_t *) MapViewOfFile(hMapFile[i],   // handle to map object
							FILE_MAP_ALL_ACCESS, // read/write permission
							0,
							0,
							BUF_SIZE);
							//576*720*4);

			hAudioMapFile[i] = ::CreateFileMapping(
                		 INVALID_HANDLE_VALUE,    // use paging file
            			 NULL,                    // default security
               			 PAGE_READWRITE,          // read/write access
               			 0,                       // maximum object size (high-order DWORD)
               			 BUF_SIZE,                // maximum object size (low-order DWORD)
						 reinterpret_cast <LPCWSTR> (current_shm_audio_path.c_str()));                 // name of mapping object
			dwLastError = ::GetLastError();
			CASPAR_LOG(trace) << print() << L"CreateFileMapping audio "<< current_shm_audio_path <<"last error : " <<dwLastError;
			if (hAudioMapFile[i] == NULL) 
			{
				if (GetLastError() == ERROR_ALREADY_EXISTS)
				{
					hAudioMapFile[i]= ::OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, reinterpret_cast <LPCWSTR> (current_shm_audio_path.c_str()));
					if (hAudioMapFile[i] == NULL)
						BOOST_THROW_EXCEPTION(caspar_exception() << msg_info("Could not create file mapping object for audio"));
				}

		
			}

			shmAudio[i] = (CasparSHM_t *) MapViewOfFile(hAudioMapFile[i],   // handle to map object
			//shm = (uint8_t **) MapViewOfFile(hMapFile,   // handle to map object
							FILE_MAP_ALL_ACCESS, // read/write permission
							0,
							0,
							1920*16*4+4096);
							//576*720*4);

			if (shm[i] == NULL) {
				::CloseHandle(hMapFile[i]);
				BOOST_THROW_EXCEPTION(caspar_exception() << msg_info("Could not map video view of file"));
			}
			if (shmAudio[i] == NULL) {
				::CloseHandle(hAudioMapFile[i]);
				BOOST_THROW_EXCEPTION(caspar_exception() << msg_info("Could not map audio view of file"));
			}
		
			memset(shm[i], 0, sizeof(CasparSHM_t));
			memset(shmAudio[i], 0, 1920*16*4+4096);
		}
		buffer_index_= -1;
		
	}

	~SharedMem_consumer()
	{		
		is_running_ = false;
		for (int i = 0; i<CIRCULAR_BUFFER_SIZE; i++)
		{
			UnmapViewOfFile(shm[i]);
			UnmapViewOfFile(shmAudio[i]);
			CloseHandle(hMapFile[i]);
			CloseHandle(hAudioMapFile[i]);
		}
		frame_buffer_.try_push(make_safe<core::read_frame>());
		thread_.join();
		auto str = print();
		CASPAR_LOG(info) << str << L" Successfully Uninitialized.";	
	}
	void FindFirstBufferIndex()
	{
		int nFirstIndex = 0;
		// si le premier est rempli, il faut voir s'il y en a avant
		for (int i = 0; i< CIRCULAR_BUFFER_SIZE; i++)
		{
			CASPAR_LOG(trace) << "Sharedmem Buffer " << i << " handshake:" << shm[i]->fp.handshake ;
		}
		if (shm[0]->fp.handshake == 0)
		{
			for (int i = CIRCULAR_BUFFER_SIZE-1; i>= 0; i--)
			{
				if (shm[i]->fp.handshake == 1) 
				{
					nFirstIndex = (i+1)%CIRCULAR_BUFFER_SIZE;
					break;
				}
			}
			buffer_index_ = nFirstIndex;
			CASPAR_LOG(trace) << "Sharedmem First Buffer Index reverse " << buffer_index_;
		}
		else
		{
			for (int i = 0; i< CIRCULAR_BUFFER_SIZE; i++)
			{
				if (shm[i]->fp.handshake == 0)
				{
					nFirstIndex = i;
					break;
				}
			}
			buffer_index_ = nFirstIndex;
			CASPAR_LOG(trace) << "Sharedmem First Buffer Index " << buffer_index_;
		}

	}
	boost::unique_future<bool> send(const safe_ptr<core::read_frame>& frame)
	{
		CASPAR_VERIFY(format_desc_.height * format_desc_.width * 4 == static_cast<unsigned>(frame->image_data().size()));
		if (buffer_index_ == -1)
			FindFirstBufferIndex();
		graph_->set_value("tick-time", tick_timer_.elapsed() * format_desc_.fps * 0.5);
		int nCurrentBufferIndex = buffer_index_;
		tick_timer_.restart();
		frame_timer_.restart();
		//LockFile(hMapFile[nCurrentBufferIndex],0, 0,BUF_SIZE, 0);
		if ((shm[nCurrentBufferIndex]->fp.handshake == 0) && (shm[(nCurrentBufferIndex+1)%CIRCULAR_BUFFER_SIZE]->fp.handshake == 0))
		{
			// VIDEO
			if (!config_.key_only)
			{
				size_t bytes_to_copy=frame->image_data().size()>BUF_SIZE?BUF_SIZE:frame->image_data().size();
		
				//std::copy_n( shm->fb.begin, bytes_to_copy , frame->image_data().begin());
				std::copy_n( frame->image_data().begin(), bytes_to_copy , shm[nCurrentBufferIndex]->fb.begin);


				// AUDIO 
				auto audio_buffer  = 
				core::get_rearranged_and_mixed(
						frame->multichannel_view(),
						audio_layout_,
						audio_layout_.num_channels);
				/*for (int i = 0; i <audio_buffer.size(); i++)
				{
					shmAudio->fb.begin[i]= swap_byte_order(audio_buffer[i]);
				}*/
				std::copy_n(audio_buffer.data(), audio_buffer.size(), shmAudio[nCurrentBufferIndex]->fa.Audiobegin);
				//AddAudioTone((unsigned int*)(shmAudio->fb.begin),mCurrentSample, 1920,48000.0, 0.1,mCurrentSample%2000,31, false,16);

			}
			else
			{
				//size_t bytes_to_copy=frame->image_data().size()>BUF_SIZE?BUF_SIZE:frame->image_data().size();
		
				//std::copy_n( shm->fb.begin, bytes_to_copy , frame->image_data().begin());
				fast_memshfl(shm[nCurrentBufferIndex]->fb.begin, frame->image_data().begin(), frame->image_data().size(), 0x0F0F0F0F, 0x0B0B0B0B, 0x07070707, 0x03030303);
				//std::copy_n( frame->image_data().begin(), bytes_to_copy , shm->fb.begin);
			}
			shm[nCurrentBufferIndex]->fp.handshake = 1;
			buffer_index_=(++buffer_index_)%CIRCULAR_BUFFER_SIZE;
			graph_->set_value("frame-time", frame_timer_.elapsed() * format_desc_.fps * 0.5);
			graph_->set_text(print() + L" (Buff " + boost::lexical_cast<std::wstring>(nCurrentBufferIndex)+ L" - "+boost::lexical_cast<std::wstring>(shmAudio[nCurrentBufferIndex]->fa.Audiobegin)+ L" - "+boost::lexical_cast<std::wstring>(shmAudio[nCurrentBufferIndex]->fa.Audiobegin[0]) + L")");
		}
		else
		{
			graph_->set_tag("dropped-frame");
		}
		//UnlockFile(hMapFile[nCurrentBufferIndex],0, 0,BUF_SIZE, 0);
		return wrap_as_future(true);
	}

	
	std::wstring channel_and_format() const
	{
		return L"[" + boost::lexical_cast<std::wstring>(channel_index_) + L"|" + format_desc_.name + L"]";
	}
	std::wstring print() const
	{
		return config_.name + L" - "+ boost::lexical_cast<std::wstring>(config_.as_admin) +boost::lexical_cast<std::wstring>(config_.device_index) + L" " + channel_and_format();
		/*
		if (config_.keyer == configuration::external_separate_device_keyer)
			return model_name_ + L" [" + boost::lexical_cast<std::wstring>(channel_index_) + L"-" +
				boost::lexical_cast<std::wstring>(config_.device_index) +
				L"&&" +
				boost::lexical_cast<std::wstring>(config_.key_device_index()) +
				L"|" +
				format_desc_.name + L"]";
		else
			return model_name_ + L" [" + boost::lexical_cast<std::wstring>(channel_index_) + L"-" +
				boost::lexical_cast<std::wstring>(config_.device_index) + L"|" +  format_desc_.name + L"]";*/
	}
};


struct SharedMem_consumer_proxy : public core::frame_consumer
{
	const configuration						config_;
	com_context<SharedMem_consumer>	context_;
	std::vector<size_t>						audio_cadence_;
	core::video_format_desc					format_desc_;
public:

	SharedMem_consumer_proxy(const configuration& config)
		: config_(config)
		, context_(L"SharedMem_consumer[" + boost::lexical_cast<std::wstring>(config.device_index) + L"]")
	{
	}

	~SharedMem_consumer_proxy()
	{
		if(context_)
		{
			auto str = print();
			context_.reset();
			CASPAR_LOG(info) << str << L" Successfully Uninitialized.";	
		}
	}

	// frame_consumer
	
	virtual void initialize(
			const core::video_format_desc& format_desc,
			const core::channel_layout& audio_channel_layout,
			int channel_index) override
	{
		context_.reset([&]{return new SharedMem_consumer(config_, format_desc, channel_index);});

		audio_cadence_ = format_desc.audio_cadence;		
		format_desc_ = format_desc;

		CASPAR_LOG(info) << print() << L" Successfully Initialized.";	
	}
	
	virtual boost::unique_future<bool> send(const safe_ptr<core::read_frame>& frame) override
	{
		CASPAR_VERIFY(audio_cadence_.front() * frame->num_channels() == static_cast<size_t>(frame->audio_data().size()));
		boost::range::rotate(audio_cadence_, std::begin(audio_cadence_)+1);

		return context_->send(frame);
	}
	
	virtual std::wstring print() const override
	{
		return context_ ? context_->print() : L"[SharedMem_consumer]";
	}		

	virtual boost::property_tree::wptree info() const override
	{
		boost::property_tree::wptree info;
		info.add(L"type", L"SharedMem-consumer");
		info.add(L"key-only", config_.key_only);
		info.add(L"device", config_.device_index);
		
		/*
		if (config_.keyer == configuration::external_separate_device_keyer)
		{
			info.add(L"key-device", config_.key_device_index());
		}

		info.add(L"low-latency", config_.latency == configuration::low_latency);
		info.add(L"embedded-audio", config_.embedded_audio);
		info.add(L"presentation-frame-age", presentation_frame_age_millis());
		//info.add(L"internal-key", config_.internal_key);*/
		return info;
	}

	virtual int buffer_depth() const override
	{
		//return config_.buffer_depth();
		return -1;
	}

	virtual int index() const override
	{
		//return 300 + config_.device_index;
		return 666;
	}

	virtual int64_t presentation_frame_age_millis() const
	{
		//return context_ ? context_->current_presentation_delay_ : 0;
		return 0;
	}
	virtual bool has_synchronization_clock() const override
	{
		return false;
	}

};


safe_ptr<core::frame_consumer> create_consumer(const core::parameters& params) 
{
		if(params.size() < 1 || (params[0] != L"sharedmem"&& params[0] != L"shm"))
		return core::frame_consumer::empty();
	
	configuration config;
		
	if(params.size() > 1)
		config.device_index =
		lexical_cast_or_default<int>(params[1], config.device_index);

	config.device_index = params.get(L"DEVICE", config.device_index);
	config.key_only				= params.get(L"key-only",			config.key_only);
	config.as_admin= params.get(L"as-admin",			config.as_admin);
	return make_safe<SharedMem_consumer_proxy>(config);
	
}

safe_ptr<core::frame_consumer> create_consumer(const boost::property_tree::wptree& ptree) 
{
		configuration config;
	config.name				= ptree.get(L"name",	 config.name);
	config.device_index		= ptree.get(L"device",   config.device_index+1)-1;
	config.key_only			= ptree.get(L"key-only", config.key_only);
	config.as_admin			= ptree.get(L"as-admin", config.as_admin);
	
	return make_safe<SharedMem_consumer_proxy>(config);
	
}

}}

