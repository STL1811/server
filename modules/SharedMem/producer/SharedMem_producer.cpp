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
* Author: STL
*/

#include "../stdafx.h"

#include "SharedMem_producer.h"

//#include "../interop/DeckLinkAPI_h.h"
#include "../util/util.h"
//#include "../util/decklink_allocator.h"

#include "../../ffmpeg/producer/filter/filter.h"
#include "../../ffmpeg/producer/util/util.h"
#include "../../ffmpeg/producer/muxer/frame_muxer.h"
#include "../../ffmpeg/producer/muxer/display_mode.h"
#include "../../ffmpeg/producer/audio/audio_resampler.h"

#include <common/concurrency/com_context.h>
#include <common/diagnostics/graph.h>
#include <common/exception/exceptions.h>
#include <common/exception/win32_exception.h>
#include <common/log/log.h>
#include <common/memory/memclr.h>

#include <core/parameters/parameters.h>
#include <core/monitor/monitor.h>
#include <core/mixer/write_frame.h>
#include <core/mixer/audio/audio_util.h>
#include <core/producer/frame/frame_transform.h>
#include <core/producer/frame/frame_factory.h>


#include <tbb/concurrent_queue.h>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/timer.hpp>

#if defined(_MSC_VER)
#pragma warning (push)
#pragma warning (disable : 4244)
#endif
extern "C" 
{
	#define __STDC_CONSTANT_MACROS
	#define __STDC_LIMIT_MACROS
	#include <libavcodec/avcodec.h>
}
#if defined(_MSC_VER)
#pragma warning (pop)
#endif

#pragma warning(push)
#pragma warning(disable : 4996)



#pragma warning(push)

#include <functional>

namespace caspar { namespace SharedMem {
		
class SharedMem_producer : boost::noncopyable
{	
	/*
	core::monitor::subject										monitor_subject_;
	safe_ptr<diagnostics::graph>								graph_;
	boost::timer												tick_timer_;
	boost::timer												frame_timer_;

	std::unique_ptr<thread_safe_decklink_allocator>				allocator_;
	CComPtr<IDeckLink>											decklink_;
	CComQIPtr<IDeckLinkInput>									input_;
	CComQIPtr<IDeckLinkAttributes>								attributes_;
	
	const std::wstring											model_name_;
	const size_t												device_index_;
	const std::wstring											filter_;
	
	core::video_format_desc										format_desc_;
	std::vector<size_t>											audio_cadence_;
	boost::circular_buffer<size_t>								sync_buffer_;
	ffmpeg::frame_muxer											muxer_;
			
	tbb::atomic<int>											hints_;
	safe_ptr<core::frame_factory>								frame_factory_;

	tbb::concurrent_bounded_queue<safe_ptr<core::basic_frame>>	frame_buffer_;

	std::exception_ptr											exception_;	
	int															num_input_channels_;
	core::channel_layout										audio_channel_layout_;*/
	
	safe_ptr<core::basic_frame> frame_;
	safe_ptr<core::frame_factory> frame_factory_;
	const int device_index_;
	int frame_index_;
	core::monitor::subject		monitor_subject_;
	const std::wstring											filter_;
	ffmpeg::frame_muxer											muxer_;
	safe_ptr<diagnostics::graph>								graph_;

	HANDLE hMapFile[CIRCULAR_BUFFER_SIZE];
	HANDLE hAudioMapFile[CIRCULAR_BUFFER_SIZE];
	std::wstring shm_path;
	CasparSHM_t *shm[CIRCULAR_BUFFER_SIZE];
	std::wstring shm_audio_path;
	CasparSHM_t *shmAudio[CIRCULAR_BUFFER_SIZE];
	core::video_format_desc										format_desc_;

	core::channel_layout										audio_channel_layout_;
	tbb::concurrent_bounded_queue<safe_ptr<core::basic_frame>>	frame_buffer_;
	int buffer_index_;
	std::shared_ptr<ffmpeg::audio_resampler>		swr_;
public:
	SharedMem_producer(
			const core::video_format_desc& format_desc,
			const core::channel_layout& audio_channel_layout,
			size_t device_index,
			const safe_ptr<core::frame_factory>& frame_factory,
			const std::wstring& filter,
			std::size_t buffer_depth, 
			bool as_admin)
		: device_index_(device_index)
		, filter_(filter)
		, format_desc_(format_desc)
		, muxer_(format_desc.fps, frame_factory, false, audio_channel_layout, filter, ffmpeg::filter::is_deinterlacing(filter))
		, frame_factory_(frame_factory)
		, audio_channel_layout_(audio_channel_layout)
		, buffer_index_(-1)
	{		
		frame_index_  =0;
		DWORD dwLastError; 
		frame_buffer_.set_capacity(2);

		graph_->set_color("tick-time", diagnostics::color(0.0f, 0.6f, 0.9f));	
		graph_->set_color("late-frame", diagnostics::color(0.6f, 0.3f, 0.3f));
		graph_->set_color("frame-time", diagnostics::color(1.0f, 0.0f, 0.0f));
		graph_->set_color("dropped-frame", diagnostics::color(0.3f, 0.6f, 0.3f));
		graph_->set_color("output-buffer", diagnostics::color(0.0f, 1.0f, 0.0f));
		graph_->set_color("volume", diagnostics::color(1.0f, 0.8f, 0.1f));
		graph_->set_color("sharedbuffer", diagnostics::color(1.0f, 0.0f, 0.0f));
		graph_->set_color("framecounter", diagnostics::color(0.0f, 1.0f, 0.0f));
		graph_->set_text(print());
		diagnostics::register_graph(graph_);
		if (as_admin == true)
		{
			shm_path = L"Global\\LORAVideoBufferProducer" +boost::lexical_cast<std::wstring>(device_index_);
			shm_audio_path = L"Global\\LORAAudioBufferProducer" +boost::lexical_cast<std::wstring>(device_index_);
		}
		else
		{
			shm_path = L"Local\\LORAVideoBufferProducer" +boost::lexical_cast<std::wstring>(device_index_);
			shm_audio_path = L"Local\\LORAAudioBufferProducer" +boost::lexical_cast<std::wstring>(device_index_);
		}

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
		
			//if (hMapFile == NULL) 
			{
				dwLastError = ::GetLastError();
				CASPAR_LOG(trace) << print() << L"CreateFileMapping video "<< current_shm_path <<" last error : " <<dwLastError;
				if ( dwLastError == ERROR_ALREADY_EXISTS)
				{
					hMapFile[i]= ::OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE,reinterpret_cast <LPCWSTR> ( current_shm_path.c_str()));
					if (hMapFile[i] == NULL)
						BOOST_THROW_EXCEPTION(caspar_exception() << msg_info("Could not create file mapping object for audio"));
				}

		
			}
			shm[i] = (CasparSHM_t *) MapViewOfFile(hMapFile[i],   // handle to map object
							FILE_MAP_ALL_ACCESS, // read/write permission
							0,
							0,
							BUF_SIZE);
							//576*720*4);

			if (shm[i] == NULL) {
				::CloseHandle(hMapFile[i]);
				BOOST_THROW_EXCEPTION(caspar_exception() << msg_info("Could not map view of file"));
			}
			//if ( dwLastError != ERROR_ALREADY_EXISTS)			
				memset(shm[i], 0, sizeof(CasparSHM_t));
				shm[i]->fp.handshake= 0;

			hAudioMapFile[i] = ::CreateFileMapping(
                		 INVALID_HANDLE_VALUE,    // use paging file
            			 NULL,                    // default security
               			 PAGE_READWRITE,          // read/write access
               			 0,                       // maximum object size (high-order DWORD)
               			 BUF_SIZE,                // maximum object size (low-order DWORD)
						 reinterpret_cast <LPCWSTR> (current_shm_audio_path.c_str()));                 // name of mapping object
		
			
			CASPAR_LOG(trace) << print() << L"CreateFileMapping audio "<<current_shm_audio_path<<" last error : " <<::GetLastError();
			//if (hAudioMapFile == NULL) 
			{
				if (dwLastError == ERROR_ALREADY_EXISTS)
				{
					hAudioMapFile[i]= ::OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, reinterpret_cast <LPCWSTR> (current_shm_audio_path.c_str()));
					if (hAudioMapFile[i] == NULL)
						BOOST_THROW_EXCEPTION(caspar_exception() << msg_info("Could not create file mapping object for audio"));
				}

		
			}
			shmAudio[i] = (CasparSHM_t *) MapViewOfFile(hAudioMapFile[i],   // handle to map object
							FILE_MAP_ALL_ACCESS, // read/write permission
							0,
							0,
							BUF_SIZE);
							//576*720*4);
			
			CASPAR_LOG(trace) << print() << L"MapViewOfFile audio last error : " <<::GetLastError();

			if (shmAudio[i] == NULL) {
				::CloseHandle(hAudioMapFile[i]);
				BOOST_THROW_EXCEPTION(caspar_exception() << msg_info("Could not map view of file"));
			}
			//if ( dwLastError != ERROR_ALREADY_EXISTS)
				memset(shmAudio[i], 0, sizeof(CasparSHM_t));
			
		}

		
				for (int i = 0; i< CIRCULAR_BUFFER_SIZE; i++)
		{
			CASPAR_LOG(trace) << "Freed Sharedmem Buffer " << i << " handshake:" << shm[i]->fp.handshake ;
		}

		CASPAR_LOG(info) << print() << L" Initialized.";
	}

	void FindFirstBufferIndex()
	{
		int nFirstIndex = 0;
		/*for(int i = 0; i< CIRCULAR_BUFFER_SIZE-1; i++)
		{
			shm[(i)%CIRCULAR_BUFFER_SIZE]->fp.handshake =0;
			
		}
		//buffer_index_ = (buffer_index_)%CIRCULAR_BUFFER_SIZE;
		for (int i = 0; i< CIRCULAR_BUFFER_SIZE; i++)
		{
			CASPAR_LOG(trace) << "Freed Sharedmem Buffer " << i << " handshake:" << shm[i]->fp.handshake ;
		}
		*/
		// si le premier est rempli, il faut voir s'il y en a avant
		for (int i = 0; i< CIRCULAR_BUFFER_SIZE; i++)
		{
			CASPAR_LOG(trace) << "Sharedmem Buffer " << i << " handshake:" << shm[i]->fp.handshake ;
		}
		if (shm[0]->fp.handshake == 1)
		{
			for (int i = CIRCULAR_BUFFER_SIZE-1; i>= 0; i--)
			{
				if (shm[i]->fp.handshake == 0) 
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
				if (shm[i]->fp.handshake == 1)
				{
					nFirstIndex = i;
					break;
				}
			}
			buffer_index_ = nFirstIndex;
			CASPAR_LOG(trace) << "Sharedmem First Buffer Index " << buffer_index_;
		}


		CASPAR_LOG(trace) << " New Sharedmem First Buffer Index " << buffer_index_;



	}

	~SharedMem_producer()
	{
		for (int i = 0; i<CIRCULAR_BUFFER_SIZE; i++)
		{
		UnmapViewOfFile(shm[i]);
		UnmapViewOfFile(shmAudio[i]);
		CloseHandle(hMapFile[i]);
		CloseHandle(hAudioMapFile[i]);
		}
		CASPAR_LOG(info) << print() << L" Destroyed.";
	}
	
	safe_ptr<core::basic_frame> get_frame(int hints)
	{
		try
		{
			if (buffer_index_ == -1)
				FindFirstBufferIndex();

			auto format_desc = format_desc_;
			int nCurrentBufferIndex = buffer_index_;
			
			if ((shm[nCurrentBufferIndex]->fp.handshake == 1) && (shm[(nCurrentBufferIndex+1)%CIRCULAR_BUFFER_SIZE]->fp.handshake == 1))
			{
				// verrue expres pour jouer du NTSC 30fps dans du PAL 25fps
				/*frame_index_ = (frame_index_ + 1)%5;
				if ((frame_index_ == 0) &&(shm[(nCurrentBufferIndex+1)%CIRCULAR_BUFFER_SIZE]->fp.handshake == 1) && (shm[(nCurrentBufferIndex+2)%CIRCULAR_BUFFER_SIZE]->fp.handshake == 1))
				{
					shm[nCurrentBufferIndex]->fp.handshake = 0;
					buffer_index_=(++buffer_index_)%CIRCULAR_BUFFER_SIZE;
					nCurrentBufferIndex = buffer_index_;
				}*/

				core::pixel_format_desc desc;
				//desc.pix_fmt = (caspar::core::pixel_format::type)shm->fp.pixelformat;
				// il faut le mettreici aussi, le pixel format
				desc.pix_fmt = caspar::core::pixel_format::type::argb;
				desc.planes.push_back(core::pixel_format_desc::plane(shm[nCurrentBufferIndex]->fp.width, shm[nCurrentBufferIndex]->fp.height, shm[nCurrentBufferIndex]->fp.channels));
//				desc.planes.push_back(core::pixel_format_desc::plane(720, 576, 4));
				// dernier en date :
				//desc.planes.push_back(core::pixel_format_desc::plane(format_desc.width, format_desc.height, 4));
			
				safe_ptr<AVFrame> av_frame(av_frame_alloc(), [](AVFrame* ptr) { av_frame_free(&ptr); });
				//avcodec_get_frame_defaults(av_frame.get());
				//size_t bytes_to_copy=frame->image_data().size()>BUF_SIZE?BUF_SIZE:frame->image_data().size();			
				av_frame->data[0]			= reinterpret_cast<uint8_t*>(shm[nCurrentBufferIndex]->fb.begin);
//				av_frame->linesize[0]		= 720*4;			
				// dernier en date :
				//av_frame->linesize[0]		= format_desc.width*4;			
				av_frame->linesize[0]		=shm[nCurrentBufferIndex]->fp.width*4;
				if (shm[nCurrentBufferIndex]->fp.pixelformat == -1 || shm[nCurrentBufferIndex]->fp.pixelformat ==0)
				{
					av_frame->format			= PIX_FMT_ARGB;
				}
				else
				{
					av_frame->format			= shm[nCurrentBufferIndex]->fp.pixelformat;
				}

//				av_frame->width				= 720;
//				av_frame->height			= 576;
				// dernier en date :
				//av_frame->width				= format_desc.width;
				//av_frame->height			= format_desc.height;
				av_frame->width				=shm[nCurrentBufferIndex]->fp.width;
				av_frame->height			=shm[nCurrentBufferIndex]->fp.height;
//				av_frame->interlaced_frame	= false;
//				av_frame->top_field_first	= 0;
				av_frame->interlaced_frame	= format_desc.field_mode != core::field_mode::progressive;
				av_frame->top_field_first	= format_desc.field_mode == core::field_mode::upper ? 1 : 0;
				av_frame->key_frame = 1;


				std::shared_ptr<core::audio_buffer> audio_buffer;
			
				// It is assumed that audio is always equal or ahead of video.
			
//					auto sample_frame_count = 1920;//audio->GetSampleFrameCount();

					/*				auto sample_frame_count = format_desc.audio_cadence[0];
				auto audio_data = reinterpret_cast<int32_t*>(shmAudio[nCurrentBufferIndex]->fb.begin);

				audio_buffer = std::make_shared<core::audio_buffer>(
								audio_data, 
								audio_data + sample_frame_count * std::min(audio_channel_layout_.num_channels, (int)shmAudio[nCurrentBufferIndex]->fp.channels));
								*/

// A vérifier au niveau du nombre d'audio en source et en destination

				auto sample_frame_count = format_desc.audio_cadence[0];
				auto audio_data = reinterpret_cast<int32_t*>(shmAudio[nCurrentBufferIndex]->fb.begin);

				// à priori ca fonctionne en NDI mais pas en AJA !!!!!!!! STL 20180213
				if ((int)shmAudio[nCurrentBufferIndex]->fp.channels == audio_channel_layout_.num_channels ||(int)shmAudio[nCurrentBufferIndex]->fp.channels ==0)
				{
					audio_buffer = std::make_shared<core::audio_buffer>(
							audio_data, 
							audio_data + sample_frame_count *  audio_channel_layout_.num_channels);
				}
				else
				{
					audio_buffer = std::make_shared<core::audio_buffer>();
					audio_buffer->resize(sample_frame_count * audio_channel_layout_.num_channels, 0);
					auto src_view = core::make_multichannel_view<int32_t>(
							audio_data, 
							audio_data + sample_frame_count * (int)shmAudio[nCurrentBufferIndex]->fp.channels, 
							audio_channel_layout_, 
							(int)shmAudio[nCurrentBufferIndex]->fp.channels);
					auto dst_view = core::make_multichannel_view<int32_t>(
							audio_buffer->begin(),
							audio_buffer->end(),
							audio_channel_layout_);

					core::rearrange(src_view, dst_view);
				}
				
				graph_->set_value("volume", shmAudio[nCurrentBufferIndex]->fa.Audiobegin[50] / std::numeric_limits<int32_t>::max());



				//auto frame = frame_factory_->create_frame(this, desc);
				// video
			
				//std::copy_n( shm->fb.begin, bytes_to_copy , frame->image_data().begin());
		// audio 
				/*bytes_to_copy=frame->audio_data().size()>BUF_SIZE?BUF_SIZE:frame->audio_data().size();
				frame->audio_data().reserve(bytes_to_copy);
				std::copy_n( shmAudio->fb.begin, bytes_to_copy , frame->audio_data().begin());*/

			
				//shm->fp.handshake = 0;
				//frame->commit();
				//frame_ = std::move(frame);
			//}
			//if(shm->fp.handshake && shm->fp.width<=1920 && shm->fp.height<=1080 && shm->fp.channels<=4) 

				graph_->set_text(print() /*+ L" (Buff " + boost::lexical_cast<std::wstring>(nCurrentBufferIndex) + L"-" + boost::lexical_cast<std::wstring>(frame_index_)+ L" " 
					+boost::lexical_cast<std::wstring>(av_frame->width)+L"x"+ boost::lexical_cast<std::wstring>(av_frame->height)
					+boost::lexical_cast<std::wstring>(shm[0]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[1]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[2]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[3]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[4]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[5]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[6]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[7]->fp.handshake) + L" " */
					/*+boost::lexical_cast<std::wstring>(shm[8]->fp.handshake) +  L" " 
					+boost::lexical_cast<std::wstring>(shm[9]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[10]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[11]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[12]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[13]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[14]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[15]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[16]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[17]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[18]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[19]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[20]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[21]->fp.handshake) +  L" " 
					+boost::lexical_cast<std::wstring>(shm[22]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[23]->fp.handshake) + L" " 
					+boost::lexical_cast<std::wstring>(shm[24]->fp.handshake) + L" " */
					+L" audio:"+ boost::lexical_cast<std::wstring>(audio_channel_layout_.num_channels)+L"-"+L" sple:"+ boost::lexical_cast<std::wstring>(sample_frame_count)+ L" "
				+L" :"+ boost::lexical_cast<std::wstring>(shmAudio[nCurrentBufferIndex]->fp.channels)+L"-"+boost::lexical_cast<std::wstring>(shmAudio[nCurrentBufferIndex]->fp.samplerate)+L" se:"+ boost::lexical_cast<std::wstring>(shmAudio[nCurrentBufferIndex]->fp.samplecount )+ L")");

				muxer_.push(av_frame, hints);	
				muxer_.push(audio_buffer);
				//boost::range::rotate(1920, std::begin(audio_cadence_)+1);
				// POLL
			/*
			for(auto frame = muxer_.poll(); frame; frame = muxer_.poll())
			{
				if(!frame_buffer_.try_push(make_safe_ptr(frame)))
				{
					auto dummy = core::basic_frame::empty();
					frame_buffer_.try_pop(dummy);

					frame_buffer_.try_push(make_safe_ptr(frame));

					graph_->set_tag("dropped-frame");
				}
			}


					if(!frame_buffer_.try_pop(frame_))
			graph_->set_tag("late-frame");

			*/
			auto frame = muxer_.poll();
			frame_ = make_safe_ptr(frame);
			//graph_->set_text(print()+ L" (Audio " + boost::lexical_cast<std::wstring>(audio_channel_layout_.num_channels));
			// verrue expres pour faire du 30fps dans du 50fps
			//if (frame_index_ == 1 || frame_index_ == 3 || frame_index_ == 4)
			//{
				shm[nCurrentBufferIndex]->fp.handshake = 0;
				buffer_index_=(++buffer_index_)%CIRCULAR_BUFFER_SIZE;
			//}
			//frame_index_ = (frame_index_ + 1)%5;
			//graph_->set_text(print() + L" (Buff Idx" + boost::lexical_cast<std::wstring>(nCurrentBufferIndex)+ L")");

			}
			else
			{
				CASPAR_LOG(trace) << " plus rien dans la memoire" << buffer_index_ << " "<< boost::lexical_cast<std::wstring>(shm[0]->fp.handshake) <<" " 
					<<boost::lexical_cast<std::wstring>(shm[1]->fp.handshake) <<  L" " 
					<<boost::lexical_cast<std::wstring>(shm[2]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[3]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[4]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[5]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[6]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[7]->fp.handshake) << L" " 
					/*<<boost::lexical_cast<std::wstring>(shm[8]->fp.handshake) <<  L" " 
					<<boost::lexical_cast<std::wstring>(shm[9]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[10]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[11]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[12]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[13]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[14]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[15]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[16]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[17]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[18]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[19]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[20]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[21]->fp.handshake) <<  L" " 
					<<boost::lexical_cast<std::wstring>(shm[22]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[23]->fp.handshake) << L" " 
					<<boost::lexical_cast<std::wstring>(shm[24]->fp.handshake) << L" " */
					;
			}

			/*
		if (shm->fp.handshake)
		{
		
			core::pixel_format_desc desc;
			//desc.pix_fmt = (caspar::core::pixel_format::type)shm->fp.pixelformat;
			desc.pix_fmt = caspar::core::pixel_format::type::argb;
			//desc.planes.push_back(core::pixel_format_desc::plane(shm->fp.width, shm->fp.height, shm->fp.channels));
			desc.planes.push_back(core::pixel_format_desc::plane(720, 576, 4));
			auto frame = frame_factory_->create_frame(this, desc);
			// video
			size_t bytes_to_copy=frame->image_data().size()>(BUF_SIZE-4096)?(BUF_SIZE-4096):frame->image_data().size();
			std::copy_n( shm->fb.begin, bytes_to_copy , frame->image_data().begin());
	// audio 
			bytes_to_copy=1920 * audio_channel_layout_.num_channels;
			frame->audio_data().reserve(bytes_to_copy);
			std::copy_n( shmAudio->fb.begin, bytes_to_copy , frame->audio_data().begin());


			shm->fp.handshake = 0;
			frame->commit();
			frame_ = std::move(frame);
		}
		
		return frame_;*/
		}
		catch(...)
		{
			CASPAR_LOG_CURRENT_EXCEPTION();
			return frame_;
		}
		
		return frame_;
	
	}
	
	std::wstring print() const
	{
		return L"SharedMem_producer[" + shm_path + L"]";
		//return model_name_ + L" [" + boost::lexical_cast<std::wstring>(device_index_) + L"|" + format_desc_.name + L"]";
	}
	
			std::wstring get_device_index() const
	{
		return boost::lexical_cast<std::wstring>(device_index_) ;
	}
	core::monitor::subject& monitor_output()
	{
		return monitor_subject_;
	}
};
	
class SharedMem_producer_proxy : public core::frame_producer
{		
	safe_ptr<core::basic_frame>		last_frame_;
	com_context<SharedMem_producer>	context_;
public:

	explicit SharedMem_producer_proxy(
			const safe_ptr<core::frame_factory>& frame_factory,
			const core::video_format_desc& format_desc,
			const core::channel_layout& audio_channel_layout,
			size_t device_index,
			const std::wstring& filter_str,
			std::size_t buffer_depth,
			const bool as_admin)
		: context_(L"SharedMem_producer[" + boost::lexical_cast<std::wstring>(device_index) + L"]")
		, last_frame_(core::basic_frame::empty())
	{
		context_.reset([&]{return new SharedMem_producer(format_desc, audio_channel_layout, device_index, frame_factory, filter_str, buffer_depth, as_admin);}); 
	}
	
	// frame_producer
				
	virtual safe_ptr<core::basic_frame> receive(
			int hints) override
	{
		auto frame = context_->get_frame(hints);
		if(frame != core::basic_frame::late())
			last_frame_ = frame;
		return frame;
	}

	virtual safe_ptr<core::basic_frame> last_frame() const override
	{
		return disable_audio(last_frame_);
	}
	
	virtual uint32_t nb_frames() const override
	{
		return (uint32_t)-1;
	}
	
	std::wstring print() const override
	{
		return context_->print();
	}
	virtual boost::property_tree::wptree info() const override
	{
		boost::property_tree::wptree info;
		info.add(L"type", L"SharedMem-producer");
		// STL 20151022 mettre le No du device dans les remontées
//		info.add(L"filename",	context_->get_device_index());//		boost::lexical_cast<std::wstring>(context_->device_index_));
		info.add(L"device",	context_->get_device_index());

		return info;
	}

	core::monitor::subject& monitor_output()
	{
		return context_->monitor_output();
	}
};

safe_ptr<core::frame_producer> create_producer(
		const safe_ptr<core::frame_factory>& frame_factory,
		const core::parameters& params)
{
	/*if(params.empty() || !boost::iequals(params[0], "decklink"))
		return core::frame_producer::empty();

	auto device_index	= params.get(L"DEVICE", -1);
	if(device_index == -1)
		device_index = boost::lexical_cast<int>(params.at(1));
	auto filter_str		= params.get(L"FILTER"); 	
	auto length			= params.get(L"LENGTH", std::numeric_limits<uint32_t>::max()); 	
	auto buffer_depth	= params.get(L"BUFFER", 2); 	
	auto format_desc	= core::video_format_desc::get(params.get(L"FORMAT", L"INVALID"));
	auto audio_layout		= core::create_custom_channel_layout(
			params.get(L"CHANNEL_LAYOUT", L"STEREO"),
			core::default_channel_layout_repository());
	
	boost::replace_all(filter_str, L"DEINTERLACE", L"YADIF=0:-1");
	boost::replace_all(filter_str, L"DEINTERLACE_BOB", L"YADIF=1:-1");
	
	if(format_desc.format == core::video_format::invalid)
		format_desc = frame_factory->get_video_format_desc();
			
	return create_producer_print_proxy(
		   create_producer_destroy_proxy(
			make_safe<decklink_producer_proxy>(frame_factory, format_desc, audio_layout, device_index, filter_str, length, buffer_depth)));*/
	if(params.empty() || !boost::iequals(params[0], "SharedMem")) {
		return core::frame_producer::empty();
	}
	auto device_index	= params.get(L"DEVICE", -1);
	if(device_index == -1)
		device_index = boost::lexical_cast<int>(params.at(1));
	// POur AJA, il faut qu'il soit en PASSTHRU et pour NDI, il faut qu'il soit en STEREO !!! STL 20180214
		/*auto audio_layout		= core::create_custom_channel_layout(
			params.get(L"CHANNEL_LAYOUT", L"PASSTHRU"),
			core::default_channel_layout_repository());*/
		auto audio_layout		= core::create_custom_channel_layout(
			params.get(L"CHANNEL_LAYOUT", L"STEREO"),
			core::default_channel_layout_repository());
		auto format_desc	= core::video_format_desc::get(params.get(L"FORMAT", L"INVALID"));
		//auto format_desc	= core::video_format_desc::get(core::video_format::pal);
		if(format_desc.format == core::video_format::invalid)
			format_desc = frame_factory->get_video_format_desc();
		auto as_admin		=  params.get(L"ADMIN", false);


	return core::create_producer_destroy_proxy(
		    core::create_producer_print_proxy(
			make_safe<SharedMem_producer_proxy>(frame_factory, format_desc, audio_layout, device_index, L"", 2, as_admin))); // 2 est le buffer depth

}

}}