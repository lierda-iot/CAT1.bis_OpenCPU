import hashlib
import sys
import struct

# Define constants for the header
MAGIC = 0x4C415050  # "LAPP" in ASCII
HEADER_LEN = 56     # Size of the app_ota_header structure in bytes
VERSION = 1         # Default version value

def calculate_sha256(file_path):
    """Calculate the SHA-256 hash of a file."""
    sha256_hash = hashlib.sha256()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            sha256_hash.update(chunk)
    return sha256_hash.digest()  # Return raw bytes instead of hexdigest

def write_header_and_content(input_file_path, app_start_addr, output_file_path):
    """
    Write the app_ota_header and the original file content to a new file.
    """
    # Calculate the SHA-256 hash of the input file
    sha256_value = calculate_sha256(input_file_path)
    #print(f"SHA-256: {sha256_value.hex()}")

   
    # Get the size of the input file
    with open(input_file_path, "rb") as f:
        f.seek(0, 2)  # Move to the end of the file
        app_size = f.tell()

    current_size = struct.calcsize("<IHHIIH32s")
    padding = (4 - (current_size % 4)) % 4

    # Prepare the header data with explicit padding bytes
    header_format = "<IHHIIH32s" + "x" * padding
    header_data = struct.pack(
        header_format,
        MAGIC,
        HEADER_LEN,
        VERSION,
        int(app_start_addr, 16),
        app_size,
        0,
        sha256_value
    )

    assert len(header_data) % 4 == 0, "Header data is not 4-byte aligned"
    
    # Write the header and original file content to the output file
    with open(output_file_path, "wb") as new_file:
        new_file.write(header_data)  # Write the header first

        # Append the original file content
        with open(input_file_path, "rb") as original_file:
            while True:
                chunk = original_file.read(4096)
                if not chunk:
                    break
                new_file.write(chunk)

if __name__ == "__main__":
    # Check command-line arguments
    #print(sys.argv)
    if len(sys.argv) != 4:
        print("Usage: python script.py <input_file_path> <app_start_addr> <output_file_path>")
        sys.exit(1)

    input_file_path = sys.argv[1]
    app_start_addr = sys.argv[2]  # Expected in hexadecimal format (e.g., "0x1000")
    output_file_path = sys.argv[3]

    # Execute the function
    write_header_and_content(input_file_path, app_start_addr, output_file_path)
    #print(f"Header and content have been written to '{output_file_path}'")