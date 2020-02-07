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
* Author: stl
*/

#pragma once

#include <common/exception/exceptions.h>
#include <common/log/log.h>
#include <common/memory/memshfl.h>
#include <core/video_format.h>
#include <core/mixer/read_frame.h>

//#include "../interop/DeckLinkAPI_h.h"

#include <boost/lexical_cast.hpp>



#include <string>

namespace caspar { namespace SharedMem {

		
#define BUF_SIZE (2048*4*2048+4096)
#define AUDIO_BUF_SIZE (4096 + 122880)
#define CIRCULAR_BUFFER_SIZE 8
typedef union {
	struct {
		uint32_t width;
		uint32_t height;
		uint32_t pixelformat;
		uint32_t framerate_n;
		uint32_t framerate_d;
		uint32_t channels;
		uint32_t samplecount;
		uint32_t samplerate;
		uint32_t handshake;
	} fp;
	struct {
		unsigned char parms[4096];
		unsigned char begin[BUF_SIZE-4096];
	} fb;
	struct 
	{
		unsigned char parms[4096];
		uint32_t Audiobegin[1920*16];
	}fa;
} CasparSHM_t;
	
struct configuration
{
		
	std::wstring	name;
	size_t			device_index;
	bool key_only ;
	bool as_admin;
	boost::optional<int> width;
	boost::optional<int> height;

	configuration()
		: name(L"Shared Memory2 Consumer")
		, device_index(0)
		, key_only(false)
		, as_admin(false)
	{
	}
};
/*
struct configuration
{
	enum keyer_t
	{
		internal_keyer,
		external_keyer,
		external_separate_device_keyer,
		default_keyer
	};

	enum latency_t
	{
		low_latency,
		normal_latency,
		default_latency
	};

	size_t					device_index;
	size_t					key_device_idx;
	bool					embedded_audio;
	core::channel_layout	audio_layout;
	keyer_t					keyer;
	latency_t				latency;
	bool					key_only;
	size_t					base_buffer_depth;
	bool					custom_allocator;
	
	configuration()
		: device_index(1)
		, key_device_idx(0)
		, embedded_audio(false)
		, audio_layout(core::default_channel_layout_repository().get_by_name(L"STEREO"))
		, keyer(default_keyer)
		, latency(default_latency)
		, key_only(false)
		, base_buffer_depth(3)
		, custom_allocator(true)
	{
	}
	
	int buffer_depth() const
	{
		return base_buffer_depth + (latency == low_latency ? 0 : 1) + (embedded_audio ? 1 : 0);
	}

	size_t key_device_index() const
	{
		return key_device_idx == 0 ? device_index + 1 : key_device_idx;
	}

	int num_out_channels() const
	{
		if (audio_layout.num_channels <= 2)
			return 2;
		
		if (audio_layout.num_channels <= 8)
			return 8;

		return 16;
	}
};
*/
/*
static void set_latency(
		const CComQIPtr<IDeckLinkConfiguration>& config,
		configuration::latency_t latency,
		const std::wstring& print)
{		
	if (latency == configuration::low_latency)
	{
		config->SetFlag(bmdDeckLinkConfigLowLatencyVideoOutput, true);
		CASPAR_LOG(info) << print << L" Enabled low-latency mode.";
	}
	else if (latency == configuration::normal_latency)
	{			
		config->SetFlag(bmdDeckLinkConfigLowLatencyVideoOutput, false);
		CASPAR_LOG(info) << print << L" Disabled low-latency mode.";
	}
}*/
/*
static void set_keyer(
		const CComQIPtr<IDeckLinkAttributes>& attributes,
		const CComQIPtr<IDeckLinkKeyer>& decklink_keyer,
		configuration::keyer_t keyer,
		const std::wstring& print)
{
	if (keyer == configuration::internal_keyer
			|| keyer == configuration::external_separate_device_keyer) 
	{
		BOOL value = true;
		if (SUCCEEDED(attributes->GetFlag(BMDDeckLinkSupportsInternalKeying, &value)) && !value)
			CASPAR_LOG(error) << print << L" Failed to enable internal keyer.";	
		else if (FAILED(decklink_keyer->Enable(FALSE)))			
			CASPAR_LOG(error) << print << L" Failed to enable internal keyer.";			
		else if (FAILED(decklink_keyer->SetLevel(255)))			
			CASPAR_LOG(error) << print << L" Failed to set key-level to max.";
		else
			CASPAR_LOG(info) << print << L" Enabled internal keyer.";		
	}
	else if (keyer == configuration::external_keyer)
	{
		BOOL value = true;
		if (SUCCEEDED(attributes->GetFlag(BMDDeckLinkSupportsExternalKeying, &value)) && !value)
			CASPAR_LOG(error) << print << L" Failed to enable external keyer.";	
		else if (FAILED(decklink_keyer->Enable(TRUE)))			
			CASPAR_LOG(error) << print << L" Failed to enable external keyer.";	
		else if (FAILED(decklink_keyer->SetLevel(255)))			
			CASPAR_LOG(error) << print << L" Failed to set key-level to max.";
		else
			CASPAR_LOG(info) << print << L" Enabled external keyer.";			
	}
}
*/
/*
template<typename Output>
class reference_signal_detector
{
	CComQIPtr<Output> output_;
	BMDReferenceStatus last_reference_status_;
public:
	reference_signal_detector(const CComQIPtr<Output>& output)
		: output_(output)
		, last_reference_status_(static_cast<BMDReferenceStatus>(-1))
	{
	}

	template<typename Print>
	void detect_change(const Print& print)
	{
		BMDReferenceStatus reference_status;

		if (output_->GetReferenceStatus(&reference_status) != S_OK)
		{
			CASPAR_LOG(error) << print() << L" Reference signal: failed while querying status";
		}
		else if (reference_status != last_reference_status_)
		{
			last_reference_status_ = reference_status;

			if (reference_status == 0)
				CASPAR_LOG(info) << print() << L" Reference signal: not detected.";
			else if (reference_status & bmdReferenceNotSupportedByHardware)
				CASPAR_LOG(info) << print() << L" Reference signal: not supported by hardware.";
			else if (reference_status & bmdReferenceLocked)
				CASPAR_LOG(info) << print() << L" Reference signal: locked.";
			else
				CASPAR_LOG(info) << print() << L" Reference signal: Unhandled enum bitfield: " << reference_status;
		}
	}
};
*/
}}