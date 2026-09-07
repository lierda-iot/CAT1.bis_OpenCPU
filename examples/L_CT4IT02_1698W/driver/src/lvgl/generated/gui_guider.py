# Copyright 2026 NXP
# NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
# accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
# activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
# comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
# terms, then you may not retain, install, activate or otherwise use the software.

import SDL
import utime as time
import usys as sys
import lvgl as lv
import lodepng as png
import ustruct
import fs_driver

lv.init()
SDL.init(w=360,h=360)

# Register SDL display driver.
disp_buf1 = lv.disp_draw_buf_t()
buf1_1 = bytearray(360*360*4)
disp_buf1.init(buf1_1, None, len(buf1_1)//4)
disp_drv = lv.disp_drv_t()
disp_drv.init()
disp_drv.draw_buf = disp_buf1
disp_drv.flush_cb = SDL.monitor_flush
disp_drv.hor_res = 360
disp_drv.ver_res = 360
disp_drv.register()

# Regsiter SDL mouse driver
indev_drv = lv.indev_drv_t()
indev_drv.init()
indev_drv.type = lv.INDEV_TYPE.POINTER
indev_drv.read_cb = SDL.mouse_read
indev_drv.register()

fs_drv = lv.fs_drv_t()
fs_driver.fs_register(fs_drv, 'Z')

# Below: Taken from https://github.com/lvgl/lv_binding_micropython/blob/master/driver/js/imagetools.py#L22-L94

COLOR_SIZE = lv.color_t.__SIZE__
COLOR_IS_SWAPPED = hasattr(lv.color_t().ch,'green_h')

class lodepng_error(RuntimeError):
    def __init__(self, err):
        if type(err) is int:
            super().__init__(png.error_text(err))
        else:
            super().__init__(err)

# Parse PNG file header
# Taken from https://github.com/shibukawa/imagesize_py/blob/ffef30c1a4715c5acf90e8945ceb77f4a2ed2d45/imagesize.py#L63-L85

def get_png_info(decoder, src, header):
    # Only handle variable image types

    if lv.img.src_get_type(src) != lv.img.SRC.VARIABLE:
        return lv.RES.INV

    data = lv.img_dsc_t.__cast__(src).data
    if data == None:
        return lv.RES.INV

    png_header = bytes(data.__dereference__(24))

    if png_header.startswith(b'\211PNG\r\n\032\n'):
        if png_header[12:16] == b'IHDR':
            start = 16
        # Maybe this is for an older PNG version.
        else:
            start = 8
        try:
            width, height = ustruct.unpack(">LL", png_header[start:start+8])
        except ustruct.error:
            return lv.RES.INV
    else:
        return lv.RES.INV

    header.always_zero = 0
    header.w = width
    header.h = height
    header.cf = lv.img.CF.TRUE_COLOR_ALPHA

    return lv.RES.OK

def convert_rgba8888_to_bgra8888(img_view):
    for i in range(0, len(img_view), lv.color_t.__SIZE__):
        ch = lv.color_t.__cast__(img_view[i:i]).ch
        ch.red, ch.blue = ch.blue, ch.red

# Read and parse PNG file

def open_png(decoder, dsc):
    img_dsc = lv.img_dsc_t.__cast__(dsc.src)
    png_data = img_dsc.data
    png_size = img_dsc.data_size
    png_decoded = png.C_Pointer()
    png_width = png.C_Pointer()
    png_height = png.C_Pointer()
    error = png.decode32(png_decoded, png_width, png_height, png_data, png_size)
    if error:
        raise lodepng_error(error)
    img_size = png_width.int_val * png_height.int_val * 4
    img_data = png_decoded.ptr_val
    img_view = img_data.__dereference__(img_size)

    if COLOR_SIZE == 4:
        convert_rgba8888_to_bgra8888(img_view)
    else:
        raise lodepng_error("Error: Color mode not supported yet!")

    dsc.img_data = img_data
    return lv.RES.OK

# Above: Taken from https://github.com/lvgl/lv_binding_micropython/blob/master/driver/js/imagetools.py#L22-L94

decoder = lv.img.decoder_create()
decoder.info_cb = get_png_info
decoder.open_cb = open_png

def anim_x_cb(obj, v):
    obj.set_x(v)

def anim_y_cb(obj, v):
    obj.set_y(v)

def anim_width_cb(obj, v):
    obj.set_width(v)

def anim_height_cb(obj, v):
    obj.set_height(v)

def anim_img_zoom_cb(obj, v):
    obj.set_zoom(v)

def anim_img_rotate_cb(obj, v):
    obj.set_angle(v)

global_font_cache = {}
def test_font(font_family, font_size):
    global global_font_cache
    if font_family + str(font_size) in global_font_cache:
        return global_font_cache[font_family + str(font_size)]
    if font_size % 2:
        candidates = [
            (font_family, font_size),
            (font_family, font_size-font_size%2),
            (font_family, font_size+font_size%2),
            ("montserrat", font_size-font_size%2),
            ("montserrat", font_size+font_size%2),
            ("montserrat", 16)
        ]
    else:
        candidates = [
            (font_family, font_size),
            ("montserrat", font_size),
            ("montserrat", 16)
        ]
    for (family, size) in candidates:
        try:
            if eval(f'lv.font_{family}_{size}'):
                global_font_cache[font_family + str(font_size)] = eval(f'lv.font_{family}_{size}')
                if family != font_family or size != font_size:
                    print(f'WARNING: lv.font_{family}_{size} is used!')
                return eval(f'lv.font_{family}_{size}')
        except AttributeError:
            try:
                load_font = lv.font_load(f"Z:MicroPython/lv_font_{family}_{size}.fnt")
                global_font_cache[font_family + str(font_size)] = load_font
                return load_font
            except:
                if family == font_family and size == font_size:
                    print(f'WARNING: lv.font_{family}_{size} is NOT supported!')

global_image_cache = {}
def load_image(file):
    global global_image_cache
    if file in global_image_cache:
        return global_image_cache[file]
    try:
        with open(file,'rb') as f:
            data = f.read()
    except:
        print(f'Could not open {file}')
        sys.exit()

    img = lv.img_dsc_t({
        'data_size': len(data),
        'data': data
    })
    global_image_cache[file] = img
    return img

def calendar_event_handler(e,obj):
    code = e.get_code()

    if code == lv.EVENT.VALUE_CHANGED:
        source = e.get_current_target()
        date = lv.calendar_date_t()
        if source.get_pressed_date(date) == lv.RES.OK:
            source.set_highlighted_dates([date], 1)

def spinbox_increment_event_cb(e, obj):
    code = e.get_code()
    if code == lv.EVENT.SHORT_CLICKED or code == lv.EVENT.LONG_PRESSED_REPEAT:
        obj.increment()
def spinbox_decrement_event_cb(e, obj):
    code = e.get_code()
    if code == lv.EVENT.SHORT_CLICKED or code == lv.EVENT.LONG_PRESSED_REPEAT:
        obj.decrement()

def digital_clock_cb(timer, obj, current_time, show_second, use_ampm):
    hour = int(current_time[0])
    minute = int(current_time[1])
    second = int(current_time[2])
    ampm = current_time[3]
    second = second + 1
    if second == 60:
        second = 0
        minute = minute + 1
        if minute == 60:
            minute = 0
            hour = hour + 1
            if use_ampm:
                if hour == 12:
                    if ampm == 'AM':
                        ampm = 'PM'
                    elif ampm == 'PM':
                        ampm = 'AM'
                if hour > 12:
                    hour = hour % 12
    hour = hour % 24
    if use_ampm:
        if show_second:
            obj.set_text("%d:%02d:%02d %s" %(hour, minute, second, ampm))
        else:
            obj.set_text("%d:%02d %s" %(hour, minute, ampm))
    else:
        if show_second:
            obj.set_text("%d:%02d:%02d" %(hour, minute, second))
        else:
            obj.set_text("%d:%02d" %(hour, minute))
    current_time[0] = hour
    current_time[1] = minute
    current_time[2] = second
    current_time[3] = ampm

def analog_clock_cb(timer, obj):
    datetime = time.localtime()
    hour = datetime[3]
    if hour >= 12: hour = hour - 12
    obj.set_time(hour, datetime[4], datetime[5])

def datetext_event_handler(e, obj):
    code = e.get_code()
    target = e.get_target()
    if code == lv.EVENT.FOCUSED:
        if obj is None:
            bg = lv.layer_top()
            bg.add_flag(lv.obj.FLAG.CLICKABLE)
            obj = lv.calendar(bg)
            scr = target.get_screen()
            scr_height = scr.get_height()
            scr_width = scr.get_width()
            obj.set_size(int(scr_width * 0.8), int(scr_height * 0.8))
            datestring = target.get_text()
            year = int(datestring.split('/')[0])
            month = int(datestring.split('/')[1])
            day = int(datestring.split('/')[2])
            obj.set_showed_date(year, month)
            highlighted_days=[lv.calendar_date_t({'year':year, 'month':month, 'day':day})]
            obj.set_highlighted_dates(highlighted_days, 1)
            obj.align(lv.ALIGN.CENTER, 0, 0)
            lv.calendar_header_arrow(obj)
            obj.add_event_cb(lambda e: datetext_calendar_event_handler(e, target), lv.EVENT.ALL, None)
            scr.update_layout()

def datetext_calendar_event_handler(e, obj):
    code = e.get_code()
    target = e.get_current_target()
    if code == lv.EVENT.VALUE_CHANGED:
        date = lv.calendar_date_t()
        if target.get_pressed_date(date) == lv.RES.OK:
            obj.set_text(f"{date.year}/{date.month}/{date.day}")
            bg = lv.layer_top()
            bg.clear_flag(lv.obj.FLAG.CLICKABLE)
            bg.set_style_bg_opa(lv.OPA.TRANSP, 0)
            target.delete()

# Create time
time = lv.obj()
time.set_size(360, 360)
time.set_scrollbar_mode(lv.SCROLLBAR_MODE.OFF)
# Set style for time, Part: lv.PART.MAIN, State: lv.STATE.DEFAULT.
time.set_style_bg_opa(0, lv.PART.MAIN|lv.STATE.DEFAULT)
time.set_style_bg_img_src("B:MicroPython/_watch_360x360.bin", lv.PART.MAIN|lv.STATE.DEFAULT)
time.set_style_bg_img_opa(255, lv.PART.MAIN|lv.STATE.DEFAULT)
time.set_style_bg_img_recolor_opa(0, lv.PART.MAIN|lv.STATE.DEFAULT)

# Create time_digital_clock_1
time_digital_clock_1_time = [int(11), int(25), int(50), ""]
time_digital_clock_1 = lv.dclock(time, "11:25:50")
time_digital_clock_1_timer = lv.timer_create_basic()
time_digital_clock_1_timer.set_period(1000)
time_digital_clock_1_timer.set_cb(lambda src: digital_clock_cb(time_digital_clock_1_timer, time_digital_clock_1, time_digital_clock_1_time, True, False ))
time_digital_clock_1.set_pos(81, 126)
time_digital_clock_1.set_size(210, 71)
# Set style for time_digital_clock_1, Part: lv.PART.MAIN, State: lv.STATE.DEFAULT.
time_digital_clock_1.set_style_radius(0, lv.PART.MAIN|lv.STATE.DEFAULT)
time_digital_clock_1.set_style_text_color(lv.color_hex(0x4f4ce7), lv.PART.MAIN|lv.STATE.DEFAULT)
time_digital_clock_1.set_style_text_font(test_font("Antonio_Regular", 41), lv.PART.MAIN|lv.STATE.DEFAULT)
time_digital_clock_1.set_style_text_opa(255, lv.PART.MAIN|lv.STATE.DEFAULT)
time_digital_clock_1.set_style_text_letter_space(2, lv.PART.MAIN|lv.STATE.DEFAULT)
time_digital_clock_1.set_style_text_align(lv.TEXT_ALIGN.CENTER, lv.PART.MAIN|lv.STATE.DEFAULT)
time_digital_clock_1.set_style_bg_opa(0, lv.PART.MAIN|lv.STATE.DEFAULT)
time_digital_clock_1.set_style_pad_top(7, lv.PART.MAIN|lv.STATE.DEFAULT)
time_digital_clock_1.set_style_pad_right(0, lv.PART.MAIN|lv.STATE.DEFAULT)
time_digital_clock_1.set_style_pad_bottom(0, lv.PART.MAIN|lv.STATE.DEFAULT)
time_digital_clock_1.set_style_pad_left(0, lv.PART.MAIN|lv.STATE.DEFAULT)
time_digital_clock_1.set_style_shadow_width(0, lv.PART.MAIN|lv.STATE.DEFAULT)

time.update_layout()
# Create baji
baji = lv.obj()
baji.set_size(360, 360)
baji.set_scrollbar_mode(lv.SCROLLBAR_MODE.OFF)
# Set style for baji, Part: lv.PART.MAIN, State: lv.STATE.DEFAULT.
baji.set_style_bg_opa(0, lv.PART.MAIN|lv.STATE.DEFAULT)
baji.set_style_bg_img_src("B:MicroPython/_baji_360x360.bin", lv.PART.MAIN|lv.STATE.DEFAULT)
baji.set_style_bg_img_opa(255, lv.PART.MAIN|lv.STATE.DEFAULT)
baji.set_style_bg_img_recolor_opa(0, lv.PART.MAIN|lv.STATE.DEFAULT)

baji.update_layout()
# Create attuition
attuition = lv.obj()
attuition.set_size(360, 360)
attuition.set_scrollbar_mode(lv.SCROLLBAR_MODE.OFF)
# Set style for attuition, Part: lv.PART.MAIN, State: lv.STATE.DEFAULT.
attuition.set_style_bg_opa(0, lv.PART.MAIN|lv.STATE.DEFAULT)
attuition.set_style_bg_img_src("B:MicroPython/_attitude_360x360.bin", lv.PART.MAIN|lv.STATE.DEFAULT)
attuition.set_style_bg_img_opa(255, lv.PART.MAIN|lv.STATE.DEFAULT)
attuition.set_style_bg_img_recolor_opa(0, lv.PART.MAIN|lv.STATE.DEFAULT)

attuition.update_layout()
# Create attfun
attfun = lv.obj()
attfun.set_size(360, 360)
attfun.set_scrollbar_mode(lv.SCROLLBAR_MODE.OFF)
# Set style for attfun, Part: lv.PART.MAIN, State: lv.STATE.DEFAULT.
attfun.set_style_bg_opa(0, lv.PART.MAIN|lv.STATE.DEFAULT)
attfun.set_style_bg_img_src("B:MicroPython/_attitudefun_360x360.bin", lv.PART.MAIN|lv.STATE.DEFAULT)
attfun.set_style_bg_img_opa(255, lv.PART.MAIN|lv.STATE.DEFAULT)
attfun.set_style_bg_img_recolor_opa(0, lv.PART.MAIN|lv.STATE.DEFAULT)

# Create attfun_led_1
attfun_led_1 = lv.led(attfun)
attfun_led_1.set_brightness(255)
attfun_led_1.set_color(lv.color_hex(0x007fff))
attfun_led_1.set_pos(168, 168)
attfun_led_1.set_size(29, 27)

attfun.update_layout()
# Create salary
salary = lv.obj()
salary.set_size(360, 360)
salary.set_scrollbar_mode(lv.SCROLLBAR_MODE.OFF)
# Set style for salary, Part: lv.PART.MAIN, State: lv.STATE.DEFAULT.
salary.set_style_bg_opa(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary.set_style_bg_img_src("B:MicroPython/_salary_360x360.bin", lv.PART.MAIN|lv.STATE.DEFAULT)
salary.set_style_bg_img_opa(255, lv.PART.MAIN|lv.STATE.DEFAULT)
salary.set_style_bg_img_recolor_opa(0, lv.PART.MAIN|lv.STATE.DEFAULT)

salary.update_layout()
# Create positioning
positioning = lv.obj()
positioning.set_size(360, 360)
positioning.set_scrollbar_mode(lv.SCROLLBAR_MODE.OFF)
# Set style for positioning, Part: lv.PART.MAIN, State: lv.STATE.DEFAULT.
positioning.set_style_bg_opa(0, lv.PART.MAIN|lv.STATE.DEFAULT)
positioning.set_style_bg_img_src("B:MicroPython/_positioning_360x360.bin", lv.PART.MAIN|lv.STATE.DEFAULT)
positioning.set_style_bg_img_opa(255, lv.PART.MAIN|lv.STATE.DEFAULT)
positioning.set_style_bg_img_recolor_opa(0, lv.PART.MAIN|lv.STATE.DEFAULT)

positioning.update_layout()
# Create salary1
salary1 = lv.obj()
salary1.set_size(360, 360)
salary1.set_scrollbar_mode(lv.SCROLLBAR_MODE.OFF)
# Set style for salary1, Part: lv.PART.MAIN, State: lv.STATE.DEFAULT.
salary1.set_style_bg_opa(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1.set_style_bg_img_src("B:MicroPython/_salary_360x360.bin", lv.PART.MAIN|lv.STATE.DEFAULT)
salary1.set_style_bg_img_opa(255, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1.set_style_bg_img_recolor(lv.color_hex(0xffffff), lv.PART.MAIN|lv.STATE.DEFAULT)
salary1.set_style_bg_img_recolor_opa(199, lv.PART.MAIN|lv.STATE.DEFAULT)

# Create salary1_ta_1
salary1_ta_1 = lv.textarea(salary1)
salary1_ta_1.set_text("99999")
salary1_ta_1.set_placeholder_text("")
salary1_ta_1.set_password_bullet("*")
salary1_ta_1.set_password_mode(False)
salary1_ta_1.set_one_line(False)
salary1_ta_1.set_accepted_chars("")
salary1_ta_1.set_max_length(32)
salary1_ta_1.set_pos(143, 108)
salary1_ta_1.set_size(153, 37)
# Set style for salary1_ta_1, Part: lv.PART.MAIN, State: lv.STATE.DEFAULT.
salary1_ta_1.set_style_text_color(lv.color_hex(0x000000), lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_1.set_style_text_font(test_font("montserratMedium", 23), lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_1.set_style_text_opa(255, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_1.set_style_text_letter_space(2, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_1.set_style_text_align(lv.TEXT_ALIGN.CENTER, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_1.set_style_bg_opa(179, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_1.set_style_bg_color(lv.color_hex(0xffffff), lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_1.set_style_bg_grad_dir(lv.GRAD_DIR.NONE, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_1.set_style_border_width(2, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_1.set_style_border_opa(255, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_1.set_style_border_color(lv.color_hex(0x6a6363), lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_1.set_style_border_side(lv.BORDER_SIDE.FULL, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_1.set_style_shadow_width(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_1.set_style_pad_top(4, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_1.set_style_pad_right(4, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_1.set_style_pad_left(4, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_1.set_style_radius(6, lv.PART.MAIN|lv.STATE.DEFAULT)

# Set style for salary1_ta_1, Part: lv.PART.SCROLLBAR, State: lv.STATE.DEFAULT.
salary1_ta_1.set_style_bg_opa(255, lv.PART.SCROLLBAR|lv.STATE.DEFAULT)
salary1_ta_1.set_style_bg_color(lv.color_hex(0x2195f6), lv.PART.SCROLLBAR|lv.STATE.DEFAULT)
salary1_ta_1.set_style_bg_grad_dir(lv.GRAD_DIR.NONE, lv.PART.SCROLLBAR|lv.STATE.DEFAULT)
salary1_ta_1.set_style_radius(0, lv.PART.SCROLLBAR|lv.STATE.DEFAULT)

# Create salary1_ta_2
salary1_ta_2 = lv.textarea(salary1)
salary1_ta_2.set_text("99999")
salary1_ta_2.set_placeholder_text("")
salary1_ta_2.set_password_bullet("*")
salary1_ta_2.set_password_mode(False)
salary1_ta_2.set_one_line(False)
salary1_ta_2.set_accepted_chars("")
salary1_ta_2.set_max_length(32)
salary1_ta_2.set_pos(143, 175)
salary1_ta_2.set_size(153, 37)
# Set style for salary1_ta_2, Part: lv.PART.MAIN, State: lv.STATE.DEFAULT.
salary1_ta_2.set_style_text_color(lv.color_hex(0x000000), lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_2.set_style_text_font(test_font("montserratMedium", 23), lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_2.set_style_text_opa(255, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_2.set_style_text_letter_space(2, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_2.set_style_text_align(lv.TEXT_ALIGN.CENTER, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_2.set_style_bg_opa(179, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_2.set_style_bg_color(lv.color_hex(0xffffff), lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_2.set_style_bg_grad_dir(lv.GRAD_DIR.NONE, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_2.set_style_border_width(2, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_2.set_style_border_opa(255, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_2.set_style_border_color(lv.color_hex(0x6e6565), lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_2.set_style_border_side(lv.BORDER_SIDE.FULL, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_2.set_style_shadow_width(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_2.set_style_pad_top(4, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_2.set_style_pad_right(4, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_2.set_style_pad_left(4, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_ta_2.set_style_radius(6, lv.PART.MAIN|lv.STATE.DEFAULT)

# Set style for salary1_ta_2, Part: lv.PART.SCROLLBAR, State: lv.STATE.DEFAULT.
salary1_ta_2.set_style_bg_opa(255, lv.PART.SCROLLBAR|lv.STATE.DEFAULT)
salary1_ta_2.set_style_bg_color(lv.color_hex(0x2195f6), lv.PART.SCROLLBAR|lv.STATE.DEFAULT)
salary1_ta_2.set_style_bg_grad_dir(lv.GRAD_DIR.NONE, lv.PART.SCROLLBAR|lv.STATE.DEFAULT)
salary1_ta_2.set_style_radius(0, lv.PART.SCROLLBAR|lv.STATE.DEFAULT)

# Create salary1_spangroup_1
salary1_spangroup_1 = lv.spangroup(salary1)
salary1_spangroup_1.set_align(lv.TEXT_ALIGN.LEFT)
salary1_spangroup_1.set_overflow(lv.SPAN_OVERFLOW.CLIP)
salary1_spangroup_1.set_mode(lv.SPAN_MODE.BREAK)
# create spans
salary1_spangroup_1_span = salary1_spangroup_1.new_span()
salary1_spangroup_1_span.set_text("每月草料")
salary1_spangroup_1_span.style.set_text_color(lv.color_hex(0x797070))
salary1_spangroup_1_span.style.set_text_decor(lv.TEXT_DECOR.NONE)
salary1_spangroup_1_span.style.set_text_font(test_font("DAIMENG", 28))
salary1_spangroup_1.set_pos(27, 116)
salary1_spangroup_1.set_size(119, 27)
salary1_spangroup_1.refr_mode()
# Set style for salary1_spangroup_1, Part: lv.PART.MAIN, State: lv.STATE.DEFAULT.
salary1_spangroup_1.set_style_border_width(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_spangroup_1.set_style_radius(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_spangroup_1.set_style_bg_opa(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_spangroup_1.set_style_pad_top(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_spangroup_1.set_style_pad_right(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_spangroup_1.set_style_pad_bottom(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_spangroup_1.set_style_pad_left(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_spangroup_1.set_style_shadow_width(0, lv.PART.MAIN|lv.STATE.DEFAULT)

# Create salary1_spangroup_2
salary1_spangroup_2 = lv.spangroup(salary1)
salary1_spangroup_2.set_align(lv.TEXT_ALIGN.LEFT)
salary1_spangroup_2.set_overflow(lv.SPAN_OVERFLOW.CLIP)
salary1_spangroup_2.set_mode(lv.SPAN_MODE.BREAK)
# create spans
salary1_spangroup_2_span = salary1_spangroup_2.new_span()
salary1_spangroup_2_span.set_text("工作时长")
salary1_spangroup_2_span.style.set_text_color(lv.color_hex(0x707070))
salary1_spangroup_2_span.style.set_text_decor(lv.TEXT_DECOR.NONE)
salary1_spangroup_2_span.style.set_text_font(test_font("DAIMENG", 28))
salary1_spangroup_2.set_pos(27, 183)
salary1_spangroup_2.set_size(119, 37)
salary1_spangroup_2.refr_mode()
# Set style for salary1_spangroup_2, Part: lv.PART.MAIN, State: lv.STATE.DEFAULT.
salary1_spangroup_2.set_style_border_width(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_spangroup_2.set_style_radius(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_spangroup_2.set_style_bg_opa(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_spangroup_2.set_style_pad_top(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_spangroup_2.set_style_pad_right(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_spangroup_2.set_style_pad_bottom(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_spangroup_2.set_style_pad_left(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_spangroup_2.set_style_shadow_width(0, lv.PART.MAIN|lv.STATE.DEFAULT)

# Create salary1_btn_1
salary1_btn_1 = lv.btn(salary1)
salary1_btn_1_label = lv.label(salary1_btn_1)
salary1_btn_1_label.set_text("确定")
salary1_btn_1_label.set_long_mode(lv.label.LONG.WRAP)
salary1_btn_1_label.set_width(lv.pct(100))
salary1_btn_1_label.align(lv.ALIGN.CENTER, 0, 0)
salary1_btn_1.set_style_pad_all(0, lv.STATE.DEFAULT)
salary1_btn_1.set_pos(129, 250)
salary1_btn_1.set_size(100, 50)
# Set style for salary1_btn_1, Part: lv.PART.MAIN, State: lv.STATE.DEFAULT.
salary1_btn_1.set_style_bg_opa(255, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_btn_1.set_style_bg_color(lv.color_hex(0xc6d6d7), lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_btn_1.set_style_bg_grad_dir(lv.GRAD_DIR.NONE, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_btn_1.set_style_border_width(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_btn_1.set_style_radius(25, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_btn_1.set_style_shadow_width(3, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_btn_1.set_style_shadow_color(lv.color_hex(0x0d4b3b), lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_btn_1.set_style_shadow_opa(255, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_btn_1.set_style_shadow_spread(0, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_btn_1.set_style_shadow_ofs_x(1, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_btn_1.set_style_shadow_ofs_y(2, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_btn_1.set_style_text_color(lv.color_hex(0xffffff), lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_btn_1.set_style_text_font(test_font("montserratMedium", 18), lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_btn_1.set_style_text_opa(255, lv.PART.MAIN|lv.STATE.DEFAULT)
salary1_btn_1.set_style_text_align(lv.TEXT_ALIGN.CENTER, lv.PART.MAIN|lv.STATE.DEFAULT)

salary1.update_layout()

def time_event_handler(e):
    code = e.get_code()
    indev = lv.indev_get_act()
    gestureDir = lv.DIR.NONE
    if indev is not None: gestureDir = indev.get_gesture_dir()
    if (code == lv.EVENT.GESTURE and lv.DIR.LEFT == gestureDir):
        if indev is not None: indev.wait_release()
        pass
        lv.scr_load_anim(baji, lv.SCR_LOAD_ANIM.MOVE_LEFT, 200, 200, False)
    indev = lv.indev_get_act()
    gestureDir = lv.DIR.NONE
    if indev is not None: gestureDir = indev.get_gesture_dir()
    if (code == lv.EVENT.GESTURE and lv.DIR.RIGHT == gestureDir):
        if indev is not None: indev.wait_release()
        pass
        lv.scr_load_anim(positioning, lv.SCR_LOAD_ANIM.MOVE_RIGHT, 200, 200, False)
time.add_event_cb(lambda e: time_event_handler(e), lv.EVENT.ALL, None)

def baji_event_handler(e):
    code = e.get_code()
    indev = lv.indev_get_act()
    gestureDir = lv.DIR.NONE
    if indev is not None: gestureDir = indev.get_gesture_dir()
    if (code == lv.EVENT.GESTURE and lv.DIR.RIGHT == gestureDir):
        if indev is not None: indev.wait_release()
        pass
        lv.scr_load_anim(time, lv.SCR_LOAD_ANIM.MOVE_RIGHT, 200, 200, False)
    indev = lv.indev_get_act()
    gestureDir = lv.DIR.NONE
    if indev is not None: gestureDir = indev.get_gesture_dir()
    if (code == lv.EVENT.GESTURE and lv.DIR.LEFT == gestureDir):
        if indev is not None: indev.wait_release()
        pass
        lv.scr_load_anim(attuition, lv.SCR_LOAD_ANIM.MOVE_LEFT, 200, 200, False)
baji.add_event_cb(lambda e: baji_event_handler(e), lv.EVENT.ALL, None)

def attuition_event_handler(e):
    code = e.get_code()
    indev = lv.indev_get_act()
    gestureDir = lv.DIR.NONE
    if indev is not None: gestureDir = indev.get_gesture_dir()
    if (code == lv.EVENT.GESTURE and lv.DIR.LEFT == gestureDir):
        if indev is not None: indev.wait_release()
        pass
        lv.scr_load_anim(salary, lv.SCR_LOAD_ANIM.MOVE_LEFT, 200, 200, False)
    indev = lv.indev_get_act()
    gestureDir = lv.DIR.NONE
    if indev is not None: gestureDir = indev.get_gesture_dir()
    if (code == lv.EVENT.GESTURE and lv.DIR.RIGHT == gestureDir):
        if indev is not None: indev.wait_release()
        pass
        lv.scr_load_anim(baji, lv.SCR_LOAD_ANIM.MOVE_RIGHT, 200, 200, False)
attuition.add_event_cb(lambda e: attuition_event_handler(e), lv.EVENT.ALL, None)

def attfun_event_handler(e):
    code = e.get_code()
    if (code == lv.EVENT.LONG_PRESSED):
        pass
        lv.scr_load_anim(attuition, lv.SCR_LOAD_ANIM.FADE_ON, 200, 200, False)
attfun.add_event_cb(lambda e: attfun_event_handler(e), lv.EVENT.ALL, None)

def salary_event_handler(e):
    code = e.get_code()
    indev = lv.indev_get_act()
    gestureDir = lv.DIR.NONE
    if indev is not None: gestureDir = indev.get_gesture_dir()
    if (code == lv.EVENT.GESTURE and lv.DIR.RIGHT == gestureDir):
        if indev is not None: indev.wait_release()
        pass
        lv.scr_load_anim(attuition, lv.SCR_LOAD_ANIM.MOVE_RIGHT, 200, 200, False)
    indev = lv.indev_get_act()
    gestureDir = lv.DIR.NONE
    if indev is not None: gestureDir = indev.get_gesture_dir()
    if (code == lv.EVENT.GESTURE and lv.DIR.LEFT == gestureDir):
        if indev is not None: indev.wait_release()
        pass
        lv.scr_load_anim(positioning, lv.SCR_LOAD_ANIM.MOVE_LEFT, 200, 200, False)
salary.add_event_cb(lambda e: salary_event_handler(e), lv.EVENT.ALL, None)

def positioning_event_handler(e):
    code = e.get_code()
    indev = lv.indev_get_act()
    gestureDir = lv.DIR.NONE
    if indev is not None: gestureDir = indev.get_gesture_dir()
    if (code == lv.EVENT.GESTURE and lv.DIR.RIGHT == gestureDir):
        if indev is not None: indev.wait_release()
        pass
        lv.scr_load_anim(salary, lv.SCR_LOAD_ANIM.MOVE_RIGHT, 200, 200, False)
    indev = lv.indev_get_act()
    gestureDir = lv.DIR.NONE
    if indev is not None: gestureDir = indev.get_gesture_dir()
    if (code == lv.EVENT.GESTURE and lv.DIR.LEFT == gestureDir):
        if indev is not None: indev.wait_release()
        pass
        lv.scr_load_anim(time, lv.SCR_LOAD_ANIM.MOVE_LEFT, 200, 200, False)
positioning.add_event_cb(lambda e: positioning_event_handler(e), lv.EVENT.ALL, None)

# content from custom.py

# Load the default screen
lv.scr_load(time)

while SDL.check():
    time.sleep_ms(5)

