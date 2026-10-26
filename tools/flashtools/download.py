#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Lierda OpenCPU Download Tool
Auto-detect port and flash firmware to device
"""

import sys
import time
import threading
import subprocess
import argparse
import zipfile
import serial
import serial.tools.list_ports
from pathlib import Path

# Try to import py7zr, but it's optional if 7z.exe is available
try:
    import py7zr
    HAS_PY7ZR = True
except ImportError:
    HAS_PY7ZR = False

# Fix Windows console encoding issue
def _ensure_utf8_stream(name):
    """Switch sys.stdout/sys.stderr to UTF-8 without crashing in wrapped environments.

    Some hosts (IDLE, IDE consoles, pythonw, tests replacing the stream with StringIO)
    provide a stream without .buffer / .reconfigure, or set it to None. In those cases
    we leave the stream as-is instead of failing at startup.
    """
    stream = getattr(sys, name, None)
    if stream is None:
        return
    try:
        # Python 3.7+: change encoding in place, keeps line buffering and flush behavior
        if hasattr(stream, 'reconfigure'):
            stream.reconfigure(encoding='utf-8', errors='replace')
            return
        if hasattr(stream, 'buffer'):
            import io
            setattr(sys, name, io.TextIOWrapper(stream.buffer, encoding='utf-8',
                                                errors='replace', line_buffering=True))
    except (AttributeError, ValueError, OSError):
        pass


if sys.platform == 'win32':
    _ensure_utf8_stream('stdout')
    _ensure_utf8_stream('stderr')


class Colors:
    """ANSI color codes for terminal output"""
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    END = '\033[0m'
    BOLD = '\033[1m'


def print_header(text):
    """Print colored header"""
    print(f"\n{Colors.BOLD}{Colors.CYAN}{'=' * 60}{Colors.END}")
    print(f"{Colors.BOLD}{Colors.CYAN}{text}{Colors.END}")
    print(f"{Colors.BOLD}{Colors.CYAN}{'=' * 60}{Colors.END}\n")


def print_step(step_num, total_steps, text):
    """Print step information"""
    print(f"{Colors.BOLD}{Colors.BLUE}[Step {step_num}/{total_steps}] {text}{Colors.END}")


def print_success(text):
    """Print success message"""
    print(f"{Colors.GREEN}[OK] {text}{Colors.END}")


def print_error(text):
    """Print error message"""
    print(f"{Colors.RED}[ERROR] {text}{Colors.END}")


def print_warning(text):
    """Print warning message"""
    print(f"{Colors.YELLOW}[WARN] {text}{Colors.END}")


def print_info(text):
    """Print info message"""
    print(f"{Colors.CYAN}[INFO] {text}{Colors.END}")


class FlashTool:
    """Flash tool manager"""

    FLASHTOOL_SUBDIR = "FlashToolCLI_V4.1.23_260807"
    FLASHTOOL_EXE = "FlashToolCLI.exe"
    CONFIG_FILE_USB = "config_pkg_product_usb.ini"
    CONFIG_FILE_UART = "config_pkg_product_uart.ini"
    SEVENZ_EXE = "tools/7z/7z.exe"  # Relative to script_dir's parent (project root)

    def __init__(self, script_dir, mode='usb'):
        self.script_dir = Path(script_dir)
        self.flashtool_dir = None
        # Select the config file based on the download mode; every command and
        # config edit below goes through self.config_file, so this is the only switch.
        self.mode = mode
        self.config_file = self.CONFIG_FILE_UART if mode == 'uart' else self.CONFIG_FILE_USB

        # Locate 7z.exe (prefer local tool)
        self.sevenz_path = None
        # Try project root relative path
        project_root = self.script_dir.parent.parent
        sevenz_candidate = project_root / self.SEVENZ_EXE
        if sevenz_candidate.exists():
            self.sevenz_path = sevenz_candidate

    def check_and_extract(self, total_steps):
        """Check if FlashToolCLI exists, extract if needed"""
        print_step(1, total_steps, "Checking FlashToolCLI tool...")

        # Check in expected subdirectory
        expected_path = self.script_dir / self.FLASHTOOL_SUBDIR / self.FLASHTOOL_EXE
        if expected_path.exists():
            self.flashtool_dir = self.script_dir / self.FLASHTOOL_SUBDIR
            print_success(f"FlashToolCLI.exe found in: {self.FLASHTOOL_SUBDIR}")
            return True

        # Check in current directory (fallback)
        current_path = self.script_dir / self.FLASHTOOL_EXE
        if current_path.exists():
            self.flashtool_dir = self.script_dir
            print_success("FlashToolCLI.exe found in current directory")
            return True

        # Not found, need to extract
        print_info("FlashToolCLI.exe not found, extracting from archive package...")

        # Find archive file (support both .zip and .7z)
        archive_files = list(self.script_dir.glob("FlashToolCLI*.zip")) + \
                       list(self.script_dir.glob("FlashToolCLI*.7z"))
        if not archive_files:
            print_error(f"FlashToolCLI archive package (.zip or .7z) not found in: {self.script_dir}")
            return False

        archive_file = archive_files[0]
        print_info(f"Found: {archive_file.name}")

        # Extract directly to script directory
        print_info(f"Extracting to: {self.script_dir}")

        try:
            if archive_file.suffix.lower() == '.7z':
                # Extract 7z file - prefer local 7z.exe tool
                if self.sevenz_path:
                    print_info(f"Using 7z tool: {self.sevenz_path}")
                    result = subprocess.run(
                        [str(self.sevenz_path), 'x', str(archive_file), f'-o{self.script_dir}', '-y'],
                        capture_output=True,
                        text=True,
                        encoding='utf-8',
                        errors='replace'
                    )
                    if result.returncode != 0:
                        print_error(f"7z extraction failed: {result.stderr}")
                        return False
                elif HAS_PY7ZR:
                    print_info("Using py7zr module")
                    with py7zr.SevenZipFile(archive_file, mode='r') as archive:
                        archive.extractall(path=self.script_dir)
                else:
                    print_error("Cannot extract .7z file: neither 7z.exe nor py7zr module found")
                    print_error(f"Expected 7z.exe at: {self.script_dir.parent.parent / self.SEVENZ_EXE}")
                    print_error("Or install py7zr: pip install py7zr")
                    return False
            else:
                # Extract zip file
                with zipfile.ZipFile(archive_file, 'r') as zip_ref:
                    zip_ref.extractall(self.script_dir)
        except Exception as e:
            print_error(f"Failed to extract: {e}")
            return False

        # After extraction, check if FlashToolCLI.exe exists
        # First check the expected subdirectory (if archive contains it)
        if expected_path.exists():
            self.flashtool_dir = self.script_dir / self.FLASHTOOL_SUBDIR
            print_success(f"Extracted to: {self.FLASHTOOL_SUBDIR}/")
            return True
        # Then check current directory (if files extracted to root)
        elif current_path.exists():
            self.flashtool_dir = self.script_dir
            print_success("Extracted successfully")
            return True
        else:
            # Search for exe in any subdirectory (fallback for unknown structure)
            for exe_path in self.script_dir.rglob(self.FLASHTOOL_EXE):
                if exe_path.is_file():
                    self.flashtool_dir = exe_path.parent
                    rel_path = self.flashtool_dir.relative_to(self.script_dir)
                    print_success(f"Extracted to: {rel_path}/")
                    return True

            print_error("Failed to find FlashToolCLI.exe after extraction")
            return False

    def get_working_dir(self):
        """Get FlashTool working directory"""
        return self.flashtool_dir

    # Only log lines containing one of these keywords are shown; the rest are hidden.
    LOG_KEYWORDS = ('-percent', 'Flashing completed successfully!')
    # Log lines containing any of these keywords are always hidden, even if they match LOG_KEYWORDS
    LOG_BLACKLIST = ('MergeRfTable.bin', 'PRMgrCfg.json', 'agentboot.bin')
    # Known FlashToolCLI subcommands, used to name each command in the result log
    SUBCOMMANDS = ('pkg2img', 'probe', 'flasherase', 'burnbatch', 'runtmcfg', 'sysreset')
    # Kill FlashToolCLI if it produces no output (shown or hidden) for this many seconds
    NO_OUTPUT_TIMEOUT = 90

    def run_command(self, args, check=True):
        """Run FlashToolCLI command, streaming filtered output live to the terminal"""
        cmd = [str(self.flashtool_dir / self.FLASHTOOL_EXE)] + args
        print_info(f"Running: {' '.join(cmd)}")

        sys.stdout.flush()
        sys.stderr.flush()

        # Merge stderr into stdout and read line by line so we can filter in real time.
        proc = subprocess.Popen(
            cmd,
            cwd=self.flashtool_dir,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding='utf-8',
            errors='replace',
            bufsize=1,
        )

        # Watchdog: readline() blocks and pipes can't be polled on Windows,
        # so a background thread kills the process if output stalls.
        last_output = [time.time()]
        timed_out = threading.Event()
        finished = threading.Event()

        def watchdog():
            while not finished.wait(1):
                if time.time() - last_output[0] > self.NO_OUTPUT_TIMEOUT:
                    timed_out.set()
                    proc.kill()
                    return

        threading.Thread(target=watchdog, daemon=True).start()

        try:
            for line in proc.stdout:
                last_output[0] = time.time()
                if any(kw in line for kw in self.LOG_BLACKLIST):
                    continue
                if any(kw in line for kw in self.LOG_KEYWORDS):
                    print(line, end='', flush=True)
            returncode = proc.wait()
        finally:
            finished.set()
            if proc.poll() is None:
                proc.kill()
                proc.wait()

        if timed_out.is_set():
            raise RuntimeError(
                f"FlashToolCLI produced no output for {self.NO_OUTPUT_TIMEOUT}s and was killed. "
                f"Please check device connection and port."
            )

        if check and returncode != 0:
            raise RuntimeError(f"Command failed with exit code {returncode}")

        # Log the result of every command so each step is traceable.
        cmd_name = next((a for a in args if a in self.SUBCOMMANDS), 'command')
        if returncode == 0:
            print_info(f"Command '{cmd_name}' finished successfully (exit code 0)")
        else:
            print_warning(f"Command '{cmd_name}' finished with exit code {returncode}")

        return subprocess.CompletedProcess(cmd, returncode)


class PortDetector:
    """Serial port detector"""

    AT_PORT_NAME = "Lierda At Port"
    QDLOADER_PORT_NAME = "Lierda QDLoade Port"  # Note: "QDLoade" not "QDLoader"
    AT_COMMAND = "AT+ECRST=delay,599"
    DETECTION_TIMEOUT = 10  # seconds

    @staticmethod
    def find_port_by_name(name):
        """Find serial port by device name, return its device (e.g. COM7)"""
        for port in serial.tools.list_ports.comports():
            if name in (port.description or ''):
                return port.device
        return None

    @staticmethod
    def verify_qdloader_port(port_name):
        """Verify if port is Lierda QDLoader Port"""
        for port in serial.tools.list_ports.comports():
            if (PortDetector.QDLOADER_PORT_NAME in (port.description or '')
                    and port.device.upper() == port_name.upper()):
                return True
        return False

    @staticmethod
    def send_at_command(port_name, command):
        """Send AT command to serial port"""
        try:
            with serial.Serial(port_name, 115200, timeout=1) as ser:
                ser.write(f"{command}\r\n".encode())
                time.sleep(0.5)
                print_success(f"AT command sent successfully to {port_name}")
                return True
        except Exception as e:
            print_warning(f"Failed to send AT command: {e}")
            return False

    @staticmethod
    def wait_for_qdloader_port(timeout_seconds):
        """Wait for QDLoader port to appear"""
        print_info(f"Waiting for {PortDetector.QDLOADER_PORT_NAME}... ({timeout_seconds}s timeout)")

        start_time = time.time()
        while time.time() - start_time < timeout_seconds:
            remaining = int(timeout_seconds - (time.time() - start_time))
            print(f"\rChecking... ({remaining}s remaining)   ", end='', flush=True)

            port = PortDetector.find_port_by_name(PortDetector.QDLOADER_PORT_NAME)
            if port:
                print()  # New line
                print_success(f"Found {PortDetector.QDLOADER_PORT_NAME}: {port}")
                return port

            time.sleep(1)

        print()  # New line
        return None

    @staticmethod
    def auto_detect_port():
        """Auto-detect download port"""
        print_info("Attempting auto-detection...")
        print_info(f"Step 1: Searching for {PortDetector.AT_PORT_NAME}...")

        at_port = PortDetector.find_port_by_name(PortDetector.AT_PORT_NAME)

        if not at_port:
            print_warning(f"{PortDetector.AT_PORT_NAME} not found.")
            print()
            print_info("Please manually enter download mode:")
            print("  1. Power off the device")
            print("  2. Press and hold the download button")
            print("  3. Power on the device")
            print("  4. Release the button after 2 seconds")
            print()
            return PortDetector.wait_for_qdloader_port(PortDetector.DETECTION_TIMEOUT)

        print_success(f"Found {PortDetector.AT_PORT_NAME}: {at_port}")
        print_info(f"Step 2: Sending {PortDetector.AT_COMMAND} to trigger download mode...")

        PortDetector.send_at_command(at_port, PortDetector.AT_COMMAND)

        print_info("Step 3: Waiting for device to enter download mode...")
        return PortDetector.wait_for_qdloader_port(PortDetector.DETECTION_TIMEOUT)


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description='Lierda OpenCPU Full Download Tool',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s firmware.binpkg --type full --mode usb                    # Full flash: USB, auto-detect port
  %(prog)s firmware.binpkg --type full --mode usb --port COM7        # Full flash: USB, specify port
  %(prog)s firmware.binpkg --type full --mode uart --port COM7       # Full flash: UART, port required
  %(prog)s firmware.binpkg --type app --mode usb --port COM7         # App only: burn single app

Auto-detect process (usb mode only):
  1. Search for "Lierda At Port"
     - If found: Send AT+ECRST=delay,599 to enter download mode
     - If not found: Prompt for manual operation
  2. Wait for "Lierda QDLoader Port" (10 seconds timeout)
  3. Start flashing when download port detected
        """
    )

    parser.add_argument('pkg_path', help='Path to firmware file')
    parser.add_argument('--type', choices=['full', 'app'], required=True, type=str.lower,
                        help='Flash type (required, case-insensitive): full (full package flash) or app (app only flash)')
    parser.add_argument('--mode', choices=['usb', 'uart'], required=True, type=str.lower,
                        help='Download mode (required, case-insensitive): usb (boot/QDLoader auto-detect) or uart (no boot mode, --port required)')
    parser.add_argument('--port', type=str.upper,
                        help='Serial port name, e.g. COM7 (case-insensitive). Optional in usb mode (auto-detect); REQUIRED in uart mode')
    parser.add_argument('--erase', action='store_true',
                        help='Erase configured flash regions (format_path) before flashing. If omitted, the erase step is skipped')

    args = parser.parse_args()

    # Print header
    print_header("Lierda OpenCPU Full Download Tool")
    print_info(f"Flash type: {args.type}")
    print_info(f"Download mode: {args.mode}")

    # Get script directory
    script_dir = Path(__file__).parent.absolute()

    # Check if input file exists
    pkg_path = Path(args.pkg_path).absolute()
    if not pkg_path.exists():
        print_error(f"File not found: {pkg_path}")
        sys.exit(1)

    is_app_flash = (args.type == 'app')
    total_steps = 6 if is_app_flash else 5
    print_info(f"Input file: {pkg_path}")

    # Step 1: Check and extract FlashToolCLI
    flash_tool = FlashTool(script_dir, mode=args.mode)
    if not flash_tool.check_and_extract(total_steps):
        sys.exit(1)

    print()

    # Step 2: Detect port
    print_step(2, total_steps, "Checking parameters and detecting download port...")

    print_info(f"Input file: {pkg_path}")
    print()

    if args.mode == 'uart':
        # UART has no boot/download mode, so there is no QDLoader port to detect
        # or verify. The user must specify the COM port and we use it directly.
        if not args.port:
            print_error("uart mode requires a serial port, e.g. python download.py <pkg> --mode uart --port COM7")
            sys.exit(1)
        port_name = args.port
        # Auto-add COM prefix if port is pure number
        if port_name.isdigit():
            port_name = f"COM{port_name}"
            print_info(f"Port number detected, using: {port_name}")
        else:
            print_info(f"Port specified (uart, used directly without QDLoader check): {port_name}")
    elif args.port:
        port_name = args.port
        # Auto-add COM prefix if port is pure number
        if port_name.isdigit():
            port_name = f"COM{port_name}"
            print_info(f"Port number detected, using: {port_name}")
        else:
            print_info(f"Port specified: {port_name}")
        print_info("Verifying port is Lierda QDLoader Port...")

        if not PortDetector.verify_qdloader_port(port_name):
            print_error(f"Port {port_name} is not a valid Lierda QDLoader Port")
            print_error("Please ensure the device is in download mode")
            sys.exit(1)

        print_success("Port verification successful")
    else:
        print_info("Port not specified, auto-detecting...")
        port_name = PortDetector.auto_detect_port()

        if not port_name:
            print_error("Failed to detect Lierda QDLoader Port")
            print_error("\nPlease check:")
            print_error("  1. Device is connected via USB")
            print_error("  2. Device drivers are installed")
            print_error("  3. Device is in download mode")
            sys.exit(1)

    print_success(f"Using port: {port_name}")
    print()

    # Step 3: Extract package with pkg2img (required for both full and app flash)
    print_step(3, total_steps, "Extracting package...")

    # Copy binpkg file to working directory first
    pkg_filename = pkg_path.name
    target_pkg = flash_tool.get_working_dir() / pkg_filename

    print_info(f"Copying {pkg_filename} to working directory...")
    import shutil
    shutil.copy2(pkg_path, target_pkg)
    print_success(f"Package copied successfully: {pkg_filename}")
    print()

    # Update configuration file
    config_file = flash_tool.get_working_dir() / flash_tool.config_file
    print_info(f"Updating configuration: {config_file}")

    # Read config and update arg_pkg_path_val
    with open(config_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    with open(config_file, 'w', encoding='utf-8') as f:
        for line in lines:
            # Check if line contains "arg_pkg_path_val " (with space)
            if 'arg_pkg_path_val ' in line:
                f.write(f"arg_pkg_path_val = {pkg_filename}\n")
            else:
                f.write(line)

    print_success(f"Configuration updated: arg_pkg_path_val = {pkg_filename}")

    # Always extract with pkg2img to generate imagedata.json
    print_info("Extracting package to images...")
    print_info(f"CONFIG_FILE: {flash_tool.config_file}")
    print_info(f"Working directory: {flash_tool.get_working_dir()}")

    # Remove leftover pkg_extract_tmp and imagedata.json from a previous run
    pkg_extract_tmp = flash_tool.get_working_dir() / "pkg_extract_tmp"
    if pkg_extract_tmp.exists():
        print_info(f"Removing existing extract dir: {pkg_extract_tmp}")
        shutil.rmtree(pkg_extract_tmp)

    imagedata_file = flash_tool.get_working_dir() / "imagedata.json"
    if imagedata_file.exists():
        print_info(f"Removing existing imagedata.json")
        imagedata_file.unlink()

    pkg2img_cmd = ['--cfgfile', flash_tool.config_file, 'pkg2img']
    print_info(f"pkg2img command: {pkg2img_cmd}")
    try:
        flash_tool.run_command(pkg2img_cmd)
        print_success("Package extracted successfully!")
    except RuntimeError as e:
        print_error(f"Failed to extract package: {e}")
        sys.exit(1)

    print()

    # If app-only flash, execute app flash procedure and exit
    if is_app_flash:
        print_step(4, total_steps, "Preparing app flash...")
        print()

        # Read imagedata.json to get PKGFLXAPP info
        import json
        with open(imagedata_file, 'r') as f:
            imagedata = json.load(f)

        # Find PKGFLXAPP entry
        pkgflxapp_info = None
        for img in imagedata.get('imageinfo', []):
            if img.get('type') == 'PKGFLXAPP':
                pkgflxapp_info = img
                break

        if not pkgflxapp_info:
            print_error("PKGFLXAPP entry not found in imagedata.json")
            sys.exit(1)

        start_addr = pkgflxapp_info['addr']
        flash_size = pkgflxapp_info['flashsize']

        # App flash address needs to subtract 0x800000
        flash_addr = start_addr - 0x800000

        # Convert to hex string format
        addr_hex = f"0x{flash_addr:X}"
        size_hex = f"0x{flash_size:X}"

        print_info(f"PKGFLXAPP address: {start_addr} (0x{start_addr:X})")
        print_info(f"Flash address (after -0x800000): {flash_addr} ({addr_hex})")
        print_info(f"PKGFLXAPP flash size: {flash_size} ({size_hex})")
        print()

        # Step 5: Probe device
        print_step(5, total_steps, "Probing device...")
        print_info(f"Probing device on port {port_name}...")
        probe_cmd = ['--cfgfile', flash_tool.config_file, '--port', port_name, 'probe']
        try:
            flash_tool.run_command(probe_cmd)
            print_success("Device probed successfully")
        except RuntimeError as e:
            print_error(f"Failed to probe device on port {port_name}")
            print_error("Please check device connection and download mode")
            sys.exit(1)
        print()

        # Step 6: Flash app
        print_step(6, total_steps, "Flashing app binary...")
        print()

        # Step 6.1: Erase flash
        print_info(f"Erasing flash region: addr={addr_hex}, size={size_hex}...")
        try:
            flash_tool.run_command([
                '--skipconnect', '1',
                '--cfgfile', flash_tool.config_file,
                '--port', port_name,
                'flasherase', addr_hex, size_hex,
                '--stor_type', 'ap_flash'
            ])
            print_success("Flash erase completed")
        except RuntimeError as e:
            print_error(f"Flash erase failed: {e}")
            sys.exit(1)

        print()

        # Step 6.2: Burn app
        print_info("Burning app binary...")
        try:
            flash_tool.run_command([
                '--skipconnect', '1',
                '--cfgfile', flash_tool.config_file,
                '--verbose', '1',
                '--port', port_name,
                'burnbatch',
                '--imglist', 'pkgflx0'
            ])
            print_success("App burn completed")
        except RuntimeError as e:
            print_error(f"App burn failed: {e}")
            sys.exit(1)

        print()

        # Step 6.3: System reset
        print_info("Resetting device...")
        try:
            flash_tool.run_command([
                '--skipconnect', '1',
                '--cfgfile', flash_tool.config_file,
                '--port', port_name,
                'sysreset'
            ], check=False)
            print_success("System reset completed")
        except RuntimeError:
            print_warning("System reset command failed, but app flash may have succeeded")

        print()
        print_header("App flashing completed successfully!")
        sys.exit(0)

    # Full flash flow continues here
    # Step 4: Detect device and configure
    print_step(4, total_steps, "Detecting device and configuring...")

    print_info(f"Probing device on port {port_name}...")
    print_info(f"CONFIG_FILE: {flash_tool.config_file}")
    print_info(f"port_name: {port_name}")
    probe_cmd = ['--cfgfile', flash_tool.config_file, '--port', port_name, 'probe']
    print_info(f"Probe command: {probe_cmd}")
    try:
        flash_tool.run_command(probe_cmd)
    except RuntimeError as e:
        print_error(f"Failed to probe device on port {port_name}")
        print_error("Please check:")
        print_error("  1. Device is connected")
        print_error("  2. Port name is correct")
        print_error("  3. Device is in download mode")
        sys.exit(1)

    # Read productname from imagedata.json
    if not imagedata_file.exists():
        print_error("imagedata.json not found after pkg2img extraction")
        sys.exit(1)

    import json
    with open(imagedata_file, 'r') as f:
        imagedata = json.load(f)

    # productname is nested under "pkgarg" in imagedata.json (fall back to top level)
    productname = imagedata.get('pkgarg', {}).get('productname', '') \
        or imagedata.get('productname', '')
    if not productname:
        print_error("Failed to read productname from imagedata.json")
        sys.exit(1)

    print_success(f"Detected productname: {productname}")

    # Update selected_product in config
    config_file = flash_tool.get_working_dir() / flash_tool.config_file
    with open(config_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    with open(config_file, 'w', encoding='utf-8') as f:
        for line in lines:
            if 'selected_product' in line and '=' in line:
                f.write(f"selected_product = {productname}\n")
            else:
                f.write(line)

    print_success(f"Configuration updated: selected_product = {productname}")
    print()

    # Step 5: Flash device
    print_step(5, total_steps, "Flashing device...")
    print()
    print_info(f"Start flashing: device={productname}, port={port_name}")
    print()

    # Execute download script

    # Erase step is opt-in via --erase; skip entirely when the flag is not set.
    if not args.erase:
        print_info("--erase not set, skipping flash erase step")
    else:
        # Read format_path from config
        format_path = None
        with open(config_file, 'r', encoding='utf-8') as f:
            for line in f:
                if 'format_path' in line and '=' in line:
                    format_path = line.split('=', 1)[1].strip()
                    break

        if format_path:
            format_json_file = flash_tool.get_working_dir() / format_path
            if format_json_file.exists():
                print_info(f"Format configuration: {format_path}")
                print_info("Erasing flash regions...")

                # Parse format JSON for erallum entries
                try:
                    with open(format_json_file, 'r') as f:
                        format_data = json.load(f)

                    erallum = format_data.get('erallum', [])
                    for erase_region in erallum:
                        stor_type = erase_region.get('stor_type', '')
                        begin = erase_region.get('begin', '')
                        length = erase_region.get('length', '')

                        print_info(f"Erasing: stor_type={stor_type} begin={begin} length={length}")
                        try:
                            flash_tool.run_command([
                                '--skipconnect', '1',
                                '--cfgfile', flash_tool.config_file,
                                '--port', port_name,
                                'flasherase', str(begin), str(length),
                                '--stor_type', stor_type
                            ])
                        except RuntimeError as e:
                            print_error(f"Flash erase failed: {e}")
                            sys.exit(1)

                except Exception as e:
                    print_warning(f"Failed to parse format file: {e}")
            else:
                print_warning(f"Format file not found: {format_path}, skipping erase step")
        else:
            print_warning("format_path not found in config, skipping erase step")

    print()
    print_info("Burning images to device...")

    # Check if pkg_extract_tmp has files to determine imglist
    pkg_extract_tmp = flash_tool.get_working_dir() / "pkg_extract_tmp"
    print_info(f"pkg_extract_tmp path: {pkg_extract_tmp}")
    print_info(f"pkg_extract_tmp exists: {pkg_extract_tmp.exists()}")

    entries = []
    if pkg_extract_tmp.exists():
        entries = list(pkg_extract_tmp.iterdir())
        print_info(f"pkg_extract_tmp entries ({len(entries)}):")
        for e in entries:
            kind = 'dir' if e.is_dir() else ('file' if e.is_file() else 'other')
            print_info(f"  [{kind}] {e.name}")

    # Only count regular files as images (ignore sub-directories and non-files)
    file_count = sum(1 for e in entries if e.is_file())
    print_info(f"Image file_count (regular files only): {file_count}")

    imglist = ['bootloader', 'system', 'cp_system']
    # Append one pkgflxN image per extra file beyond the base 3 images
    pkg_count = file_count - len(imglist)
    print_info(f"pkg_count (file_count - {len(imglist)}): {pkg_count}")
    if pkg_count > 0:
        imglist += [f'pkgflx{i}' for i in range(pkg_count)]
    print_info(f"Using image list ({', '.join(imglist)})...")

    print()
    print_info("Running runtime config (runtmcfg)...")
    try:
        flash_tool.run_command([
            '--port', port_name,
            '--cfgfile', flash_tool.config_file,
            '--skipconnect', '1',
            'runtmcfg'
        ])
    except RuntimeError as e:
        print_error(f"Runtime config failed: {e}")
        sys.exit(1)

    try:
        flash_tool.run_command([
            '--skipconnect', '1',
            '--cfgfile', flash_tool.config_file,
            '--verbose', '1',
            '--port', port_name,
            'burnbatch',
            '--imglist'
        ] + imglist)
    except RuntimeError as e:
        print_error(f"Burn batch failed: {e}")
        sys.exit(1)

    print()
    print_info("Resetting device...")
    try:
        flash_tool.run_command([
            '--skipconnect', '1',
            '--cfgfile', flash_tool.config_file,
            '--port', port_name,
            'sysreset'
        ], check=False)  # Don't fail if reset command fails
    except RuntimeError:
        print_warning("System reset command failed, but flashing may have succeeded")

    print()
    print_header("Flashing completed successfully!")


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print()
        print_error("Operation cancelled by user")
        sys.exit(1)
    except Exception as e:
        print()
        print_error(f"Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
