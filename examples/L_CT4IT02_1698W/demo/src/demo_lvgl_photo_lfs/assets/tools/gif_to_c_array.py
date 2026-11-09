#!/usr/bin/env python3
"""
GIF to C Array Converter for LVGL

Converts GIF files to C array format compatible with LVGL's lv_img_dsc_t structure.
The output uses LV_IMG_CF_RAW format since GIF data is stored as raw binary.

Usage:
    python gif_to_c_array.py <input_gif_file> <output_c_file> <array_name>

Example:
    python gif_to_c_array.py SAA_360_360.gif saa_360_360_gif_data.c saa_360_360_gif
"""

import sys
import os

def gif_to_c_array(input_path, output_path, array_name):
    """
    Convert GIF file to C array.
    
    Args:
        input_path: Path to input GIF file
        output_path: Path to output C file
        array_name: Base name for the C array and lv_img_dsc_t struct
    """
    # Read GIF file
    with open(input_path, 'rb') as f:
        gif_data = f.read()
    
    file_size = len(gif_data)
    
    # Parse GIF header to get dimensions
    # GIF87a/GIF89a header: 6 bytes signature + 7 bytes logical screen descriptor
    if len(gif_data) < 13:
        raise ValueError("Invalid GIF file: too short")
    
    signature = gif_data[0:6].decode('ascii')
    if signature not in ('GIF87a', 'GIF89a'):
        raise ValueError(f"Invalid GIF signature: {signature}")
    
    width = int.from_bytes(gif_data[6:8], 'little')
    height = int.from_bytes(gif_data[8:10], 'little')
    
    print(f"[INFO] GIF Signature: {signature}")
    print(f"[INFO] Dimensions: {width}x{height}")
    print(f"[INFO] File Size: {file_size} bytes")
    
    # Generate C array name
    map_name = f"{array_name}_map"
    data_name = f"{array_name}_data"
    
    # Write C file
    with open(output_path, 'w') as f:
        f.write(f"#if defined(LV_LVGL_H_INCLUDE_SIMPLE)\n")
        f.write(f"#include \"lvgl.h\"\n")
        f.write(f"#else\n")
        f.write(f"#include \"lvgl/lvgl.h\"\n")
        f.write(f"#endif\n")
        f.write(f"\n")
        f.write(f"#ifndef LV_ATTRIBUTE_MEM_ALIGN\n")
        f.write(f"#define LV_ATTRIBUTE_MEM_ALIGN\n")
        f.write(f"#endif\n")
        f.write(f"#ifndef LV_ATTRIBUTE_LARGE_CONST\n")
        f.write(f"#define LV_ATTRIBUTE_LARGE_CONST\n")
        f.write(f"#endif\n")
        f.write(f"\n")
        f.write(f"/* Converted from {os.path.basename(input_path)} */\n")
        f.write(f"/* GIF: {signature}, {width}x{height}, {file_size} bytes */\n")
        f.write(f"const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST\n")
        f.write(f"uint8_t {map_name}[] = {{\n")
        
        # Write data in chunks of 16 bytes per line
        for i in range(0, file_size, 16):
            chunk = gif_data[i:i+16]
            hex_values = [f"0x{b:02x}" for b in chunk]
            line = "    " + ", ".join(hex_values) + ","
            f.write(line + "\n")
        
        f.write(f"}};\n")
        f.write(f"\n")
        f.write(f"const lv_img_dsc_t {data_name} = {{\n")
        f.write(f"    .header.cf = LV_IMG_CF_RAW,\n")
        f.write(f"    .header.always_zero = 0,\n")
        f.write(f"    .header.reserved = 0,\n")
        f.write(f"    .header.w = {width},\n")
        f.write(f"    .header.h = {height},\n")
        f.write(f"    .data_size = sizeof({map_name}),\n")
        f.write(f"    .data = {map_name},\n")
        f.write(f"}};\n")
    
    print(f"[INFO] Output written to: {output_path}")
    print(f"[INFO] Array name: {map_name}")
    print(f"[INFO] Image descriptor: {data_name}")

def main():
    if len(sys.argv) != 4:
        print("Usage: python gif_to_c_array.py <input_gif_file> <output_c_file> <array_name>")
        print("")
        print("Example:")
        print("    python gif_to_c_array.py SAA_360_360.gif saa_360_360_gif_data.c saa_360_360_gif")
        sys.exit(1)
    
    input_path = sys.argv[1]
    output_path = sys.argv[2]
    array_name = sys.argv[3]
    
    if not os.path.exists(input_path):
        print(f"Error: Input file not found: {input_path}")
        sys.exit(1)
    
    gif_to_c_array(input_path, output_path, array_name)

if __name__ == "__main__":
    main()
