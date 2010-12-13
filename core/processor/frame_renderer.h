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
#pragma once

#include "fwd.h"

#include "../../common/utility/safe_ptr.h"

#include "../format/video_format.h"

#include <boost/noncopyable.hpp>

#include <memory>

namespace caspar { namespace core {

class frame_renderer : boost::noncopyable
{
public:
	frame_renderer(const video_format_desc& format_desc_);
		
	safe_ptr<read_frame> render(const safe_ptr<draw_frame>& frame);
private:
	struct implementation;
	std::shared_ptr<implementation> impl_;
};
typedef std::shared_ptr<frame_renderer> frame_renderer_ptr;

}}