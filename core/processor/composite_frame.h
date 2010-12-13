#pragma once

#include "fwd.h"

#include "draw_frame.h"

#include "../format/video_format.h"

#include <memory>
#include <algorithm>

namespace caspar { namespace core {
	
class composite_frame : public draw_frame
{
public:
	explicit composite_frame(std::vector<safe_ptr<draw_frame>>&& frames);
	composite_frame(safe_ptr<draw_frame>&& frame1, safe_ptr<draw_frame>&& frame2);
	
	void swap(composite_frame& other);

	composite_frame(const composite_frame& other);
	composite_frame(composite_frame&& other);

	composite_frame& operator=(const composite_frame& other);
	composite_frame& operator=(composite_frame&& other);

	static safe_ptr<composite_frame> interlace(safe_ptr<draw_frame>&& frame1, safe_ptr<draw_frame>&& frame2, video_mode::type mode);
	
	virtual const std::vector<short>& audio_data() const;

private:	
	virtual void begin_write();
	virtual void end_write();
	virtual void draw(frame_shader& shader);

	struct implementation;
	std::shared_ptr<implementation> impl_;
};
	
}}