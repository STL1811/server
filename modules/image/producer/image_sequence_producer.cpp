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
* Author: Helge Norberg, helge.norberg@svt.se
*/

#include "image_sequence_producer.h"

#include "../util/image_loader.h"
#include "../util/image_view.h"
#include "../util/image_algorithms.h"

#include <core/video_format.h>

#include <core/parameters/parameters.h>
#include <core/monitor/monitor.h>
#include <core/producer/frame/basic_frame.h>
#include <core/producer/frame/frame_factory.h>
#include <core/producer/frame/frame_transform.h>
#include <core/mixer/write_frame.h>

#include <common/env.h>
#include <common/log/log.h>
#include <common/memory/memclr.h>
#include <common/exception/exceptions.h>
#include <common/utility/tweener.h>

#include <boost/assign.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/gil/gil_all.hpp>

#include <algorithm>
#include <array>
#include <boost/math/special_functions/round.hpp>
#include <boost/scoped_array.hpp>

using namespace boost::assign;

namespace caspar { namespace image {

struct image_sequence_producer : public core::frame_producer
{	
	core::monitor::subject						monitor_subject_;
	const std::wstring							filename_;
	const safe_ptr<core::frame_factory> frame_factory_;
	std::vector<safe_ptr<core::basic_frame>>	frames_;
	std::vector<safe_ptr<core::basic_frame>>::iterator it;
	core::video_format_desc						format_desc_;
	size_t										width_;
	size_t										height_;

	double										delta_;
	double										speed_;

	int											start_offset_x_;
	int											start_offset_y_;
	bool										progressive_;
	int i;
	bool m_bPremultiply; // STL 20180125 Gestion du Premultiply pour les TGA en source

	safe_ptr<core::basic_frame>					last_frame_;
	
	explicit image_sequence_producer(
		const safe_ptr<core::frame_factory>& frame_factory, 
		const std::wstring& filename, 
		double speed,
		double duration,
		int motion_blur_px = 0,
		bool premultiply_with_alpha = false,
		bool progressive = false) 
		: filename_(filename)
		, frame_factory_(frame_factory)
		, delta_(0)
		, format_desc_(frame_factory->get_video_format_desc())
		, speed_(speed)
		, progressive_(progressive)
		, last_frame_(core::basic_frame::empty())
	{
		start_offset_x_ = 0;
		start_offset_y_ = 0;
		i = 0;
		bool bPremultiply = true;
		// STL 20180125 Gestion du Premultiply pour les TGA en source celui de l'image scroll est défini plus bas
		std::wstring filenametemp;
		do
		{
			if (i < 10) filenametemp = filename_ + L"0000" + boost::lexical_cast<std::wstring>(i)+L".tga";
			else filenametemp = filename_ + L"000" + boost::lexical_cast<std::wstring>(i)+L".tga";

			// STL >> 20180125 Gestion du Premultiply pour les TGA en source
			//if (!bPremultiply)
			{
				bPremultiply = env::properties().get(L"configuration.image.premultiplyTGA", false);
				// il faut encore vérifier que c'est un TGA, se serait dommage
				m_bPremultiply = bPremultiply;
			}
			load(load_image(filenametemp, bPremultiply));
			i++;
		//} while (boost::filesystem::exists(filenametemp));
			} while (i<=59);

		i = 0;
		
		it = frames_.begin();
		CASPAR_LOG(info) << print() << L" Initialized";
	}
	void load(const std::shared_ptr<FIBITMAP>& bitmap)
	{
		FreeImage_FlipVertical(bitmap.get());

		core::pixel_format_desc desc;
		desc.pix_fmt = core::pixel_format::bgra;
		desc.planes.push_back(core::pixel_format_desc::plane(FreeImage_GetWidth(bitmap.get()), FreeImage_GetHeight(bitmap.get()), 4));
		auto frame = frame_factory_->create_frame(this, desc);

		std::copy_n(FreeImage_GetBits(bitmap.get()), frame->image_data().size(), frame->image_data().begin());
		frame->commit();
		frames_.push_back(frame);
	}

	// frame_producer

	safe_ptr<core::basic_frame> render_frame(bool allow_eof)
	{
		if(frames_.empty() || it== frames_.end())
			return core::basic_frame::eof();
		
		auto result = make_safe<core::basic_frame>(*it);
		it++;
		return result;
	}

	safe_ptr<core::basic_frame> render_frame(bool allow_eof, bool advance_delta)
	{
		auto result = render_frame(allow_eof);

		if (advance_delta)
		{
			advance();
		}

		return result;
	}

	void advance()
	{
		delta_ += speed_;
	}

	virtual safe_ptr<core::basic_frame> receive(int) override
	{
		if (format_desc_.field_mode == core::field_mode::progressive || progressive_)
		{
			return last_frame_ = render_frame(true, true);
		}
		else
		{
			auto field1 = render_frame(true, true);
			auto field2 = render_frame(true, false);

			if (field1 != core::basic_frame::eof() && field2 == core::basic_frame::eof())
			{
				field2 = render_frame(false, true);
			}
			else
			{
				advance();
			}

			last_frame_ = field2;

			return core::basic_frame::interlace(field1, field2, format_desc_.field_mode);
		}
	}

	virtual safe_ptr<core::basic_frame> last_frame() const override
	{
		return last_frame_;
	}
		
	virtual std::wstring print() const override
	{
		return L"image_sequence_producer[" + filename_ + L"]";
	}

	virtual boost::property_tree::wptree info() const override
	{
		boost::property_tree::wptree info;
		info.add(L"type", L"image-sequence-producer");
		info.add(L"filename", filename_);
		return info;
	}

	virtual uint32_t nb_frames() const override
	{
		if(width_ == format_desc_.width)
		{
			auto length = (height_ + format_desc_.height * 2);
			return static_cast<uint32_t>(length / std::abs(speed_));// + length % std::abs(delta_));
		}
		else
		{
			auto length = (width_ + format_desc_.width * 2);
			return static_cast<uint32_t>(length / std::abs(speed_));// + length % std::abs(delta_));
		}
	}

	core::monitor::subject& monitor_output()
	{
		return monitor_subject_;
	}
};

safe_ptr<core::frame_producer> create_sequence_producer(
		const safe_ptr<core::frame_factory>& frame_factory,
		const core::parameters& params)
{
	/*static const std::vector<std::wstring> extensions = list_of
			(L"png")(L"tga")(L"bmp")(L"jpg")(L"jpeg")(L"gif")(L"tiff")(L"tif")
			(L"jp2")(L"jpx")(L"j2k")(L"j2c");*/
	std::wstring filename = env::media_folder() + L"\\" + params.at_original(0);
	
	/*auto ext = std::find_if(
			extensions.begin(),
			extensions.end(),
			[&](const std::wstring& ex) -> bool
			{					
				return boost::filesystem::is_regular_file(
						boost::filesystem::path(filename)
								.replace_extension(ex));
			});*/

	//if(ext == extensions.end())
	//	return core::frame_producer::empty();
	
	double speed = params.get(L"SPEEDSEQ", 0.0);
	//double duration = params.get(L"DURATION", 0.0);

	if(speed == 0 )
		return core::frame_producer::empty();

	int motion_blur_px = params.get(L"BLUR", 0);
	bool premultiply_with_alpha = params.has(L"PREMULTIPLY");
	bool progressive = params.has(L"PROGRESSIVE");

	return create_producer_print_proxy(make_safe<image_sequence_producer>(
			frame_factory, 
			filename , 
			-speed, 
			-0, 
			motion_blur_px, 
			premultiply_with_alpha,
			progressive));
}

}}