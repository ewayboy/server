/*
* copyright (c) 2010 Sveriges Television AB <info@casparcg.com>
*
*  This file is part of CasparCG.
*
*    CasparCG is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    CasparCG is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.

*    You should have received a copy of the GNU General Public License
*    along with CasparCG.  If not, see <http://www.gnu.org/licenses/>.
*
*/
#include "../StdAfx.h"

#ifdef _MSC_VER
#pragma warning (disable : 4244)
#endif

#include "frame_consumer_device.h"

#include "../video_channel_context.h"

#include "../video_format.h"
#include "../mixer/gpu/ogl_device.h"
#include "../mixer/read_frame.h"

#include <common/concurrency/executor.h>
#include <common/diagnostics/graph.h>
#include <common/utility/assert.h>
#include <common/utility/timer.h>
#include <common/memory/memshfl.h>

#include <boost/circular_buffer.hpp>
#include <boost/timer.hpp>

namespace caspar { namespace core {
	
struct frame_consumer_device::implementation
{	
	typedef std::pair<safe_ptr<const read_frame>, safe_ptr<const read_frame>> fill_and_key;
	
	video_channel_context& channel_;

	std::map<int, safe_ptr<frame_consumer>> consumers_;
	typedef std::map<int, safe_ptr<frame_consumer>>::value_type layer_t;
	
	high_prec_timer timer_;
	
	safe_ptr<diagnostics::graph> diag_;

	boost::timer frame_timer_;
	boost::timer tick_timer_;
		
public:
	implementation(video_channel_context& video_channel) 
		: channel_(video_channel)
		, diag_(diagnostics::create_graph(std::string("frame_consumer_device")))
	{		
		diag_->add_guide("frame-time", 0.5f);	
		diag_->set_color("frame-time", diagnostics::color(1.0f, 0.0f, 0.0f));
		diag_->set_color("tick-time", diagnostics::color(0.1f, 0.7f, 0.8f));
	}

	std::pair<size_t, size_t> buffer_depth()
	{		
		auto depth_comp = [](const layer_t& lhs, const layer_t& rhs){ return lhs.second->buffer_depth() < rhs.second->buffer_depth(); };
		auto min = std::min_element(consumers_.begin(), consumers_.end(), depth_comp)->second->buffer_depth();
		auto max = std::max_element(consumers_.begin(), consumers_.end(), depth_comp)->second->buffer_depth();
		CASPAR_ASSERT(max >= min);
		return std::make_pair(min, max);
	}

	void add(int index, safe_ptr<frame_consumer>&& consumer)
	{		
		consumer->initialize(channel_.get_format_desc());
		channel_.execution().invoke([&]
		{
			this->remove(index);
			consumers_.insert(std::make_pair(index, consumer));

			CASPAR_LOG(info) << print() << L" " << consumer->print() << L" Added.";
		});
	}

	void remove(int index)
	{
		channel_.execution().invoke([&]
		{
			auto it = consumers_.find(index);
			if(it != consumers_.end())
			{
				CASPAR_LOG(info) << print() << L" " << it->second->print() << L" Removed.";
				consumers_.erase(it);
			}
		});
	}
						
	void execute(const safe_ptr<read_frame>& frame)
	{		
		if(!has_synchronization_clock())
			timer_.tick(1.0/channel_.get_format_desc().fps);

		frame_timer_.restart();
				
		auto fill = frame;
		auto key = get_key_frame(frame);

		for_each_consumer([&](safe_ptr<frame_consumer>& consumer)
		{
			if(consumer->get_video_format_desc() != channel_.get_format_desc())
				consumer->initialize(channel_.get_format_desc());

			auto frame = consumer->key_only() ? key : fill;

			if(static_cast<size_t>(frame->image_data().size()) == consumer->get_video_format_desc().size)
				consumer->send(frame);
		});

		diag_->update_value("frame-time", frame_timer_.elapsed()*channel_.get_format_desc().fps*0.5);
			
		diag_->update_value("tick-time", tick_timer_.elapsed()*channel_.get_format_desc().fps*0.5);
		tick_timer_.restart();
	}

private:

	bool has_synchronization_clock()
	{
		return std::any_of(consumers_.begin(), consumers_.end(), [](const decltype(*consumers_.begin())& p)
		{
			return p.second->has_synchronization_clock();
		});
	}

	safe_ptr<const read_frame> get_key_frame(const safe_ptr<const read_frame>& frame)
	{
		bool has_key_only = std::any_of(consumers_.begin(), consumers_.end(), [](const decltype(*consumers_.begin())& p)
		{
			return p.second->key_only();
		});

		if(has_key_only)
		{
			// Currently do key_only transform on cpu. Unsure if the extra 400MB/s (1080p50) overhead is worth it to do it on gpu.
			auto key_data = channel_.ogl().create_host_buffer(frame->image_data().size(), host_buffer::write_only);				
			fast_memsfhl(key_data->data(), frame->image_data().begin(), frame->image_data().size(), 0x0F0F0F0F, 0x0B0B0B0B, 0x07070707, 0x03030303);
			std::vector<int16_t> audio_data(frame->audio_data().begin(), frame->audio_data().end());
			return make_safe<read_frame>(std::move(key_data), std::move(audio_data));
		}
		
		return read_frame::empty();
	}
		
	void for_each_consumer(const std::function<void(safe_ptr<frame_consumer>& consumer)>& func)
	{
		auto it = consumers_.begin();
		while(it != consumers_.end())
		{
			try
			{
				func(it->second);
				++it;
			}
			catch(...)
			{
				CASPAR_LOG_CURRENT_EXCEPTION();
				consumers_.erase(it++);
				CASPAR_LOG(error) << print() << L" " << it->second->print() << L" Removed.";
			}
		}
	}

	std::wstring print() const
	{
		return L"frame_consumer_device";
	}
};

frame_consumer_device::frame_consumer_device(video_channel_context& video_channel) 
	: impl_(new implementation(video_channel)){}
void frame_consumer_device::add(int index, safe_ptr<frame_consumer>&& consumer){impl_->add(index, std::move(consumer));}
void frame_consumer_device::remove(int index){impl_->remove(index);}
void frame_consumer_device::execute(const safe_ptr<read_frame>& frame) {impl_->execute(frame); }
}}