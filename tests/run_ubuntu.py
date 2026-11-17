# NoirVisor - Hardware-Accelerated Hypervisor solution
#
# Copyright (c) Zero Tang, 2018-2026. All rights reserved.
#
# This script boots the Ubuntu VM in QEMU with NoirVisor, logs in automatically,
# runs the Linux Test Project (LTP) with kirk, prints the summary and shuts down the VM.
#
# The disk image is never modified: QEMU runs the disk with `snapshot=on`, and
# the UEFI variable store is copied into a temporary directory before use.
#
# This program is distributed in the hope that it will be useful, but
# without any warranty (no matter implied warranty of merchantability or
# fitness for a particular purpose, etc.).

import codecs
import os
import re
import shlex
import shutil
import socket
import subprocess
import sys
import tempfile
import time

DEFAULT_DISK="compressed-disk.qcow2"
DEFAULT_USER="qemu"
DEFAULT_PASSWORD="qemu@123"
LOGIN_ATTEMPTS=3
LTP_DIR="/opt/ltp"
# Besides the general load, the following suites exercise code paths of the hypervisor:
# - math, crypto: FPU and SIMD state must survive VM-exits.
# - crashme: random instructions (e.g.: CPUID with random leaves) and exceptions in user mode, and random syscalls.
# - cpuhotplug: bringing a processor online sends INIT and Startup-IPI, which the hypervisor virtualizes. (Requires 2+ processors.)
# - noirvisor: individual tests from other suites, written by this script. (See NOIRVISOR_SUITE.)
DEFAULT_LTP_SUITES="smoketest mm hugetlb sched nptl kernel_misc crypto math crashme cpuhotplug noirvisor"
# Skipped tests by default:
# - oom*: exhaust guest memory and take a long time under TCG without exercising the hypervisor much.
# - hugemmap15, starvation, rtc01, zram01: known to fail in this VM even without NoirVisor.
# - irqbalance01: fails with 2+ processors even without NoirVisor because this VM has no irqbalance service.
# - cpuhotplug05, cpuhotplug06, cpuhotplug07: require sar, top, and kernel source respectively.
# - block_dev: its test module unregisters block devices with wrong names on purpose, which makes the kernel warn
#   (and sets the W taint flag). This would hide real warnings from the kernel check.
DEFAULT_LTP_SKIP="^(oom.*|hugemmap15|starvation|rtc01|zram01|irqbalance01|cpuhotplug0[5-7]|block_dev)$"
# The "noirvisor" suite, which collects tests from other suites that hit the hypervisor's code paths directly.
NOIRVISOR_SUITE=[
	# Checks XMM registers for corruption while spinning. Also relies on XSAVE sizes reported by CPUID leaf 0xD.
	"ptrace07 ptrace07",
	# MOV SS on a watched address followed by INT3: blocking by MOV SS with a pending #DB.
	"ptrace09 ptrace09",
	# FPU state must survive signals.
	"signal06 signal06",
	# The vDSO uses RDPID/RDTSCP (i.e.: the TSC_AUX MSR) or the limit of the per-CPU GDT entry.
	"getcpu01 getcpu01",
	"getcpu02 getcpu02",
	# Expedited memory barriers send IPIs to all processors.
	"membarrier01 membarrier01",
	# CPUID faulting. (Only if the processor reports it.)
	"arch_prctl01 arch_prctl01",
	# NMIs from performance counters while the kernel is on espfix or LDT stacks. (Only if hardware perf events are available.)
	"cve-2015-3290 cve-2015-3290",
]
# Kernel messages that indicate a hypervisor problem even if every test passed.
KERNEL_LOG_PATTERN=re.compile("|".join([
	# NMIs that the guest did not expect (e.g.: an NMI that the hypervisor used to kick a processor leaked into the guest).
	r"NMI received for unknown reason",
	r"soft lockup",
	r"hard LOCKUP",
	r"rcu.*detected stall",
	# Both "WARNING: CPU: 0 PID: 1 at file:line" and the newer "WARNING: file:line at function, CPU#0: task".
	r"WARNING: ",
	r"BUG: ",
	# Oops headers (e.g.: "general protection fault: 0000 [#1] SMP").
	r"Oops",
	r"\[#[0-9]+\]",
	r"Kernel panic",
	r"Hardware Error",
	r"Machine check",
	r"clocksource.*unstable",
	# A RDMSR or WRMSR in the kernel raised #GP.
	r"unchecked MSR access error",
]))
# Taint flags that indicate a kernel problem: (bit, flag, description).
# Other flags (e.g.: O and E for LTP's test modules) are expected.
BAD_TAINT_FLAGS=[
	(4,"M","machine check"),
	(5,"B","bad page"),
	(7,"D","kernel died"),
	(9,"W","kernel warning"),
	(14,"L","soft lockup"),
]
# A distinctive shell prompt so that the script can tell when a command completes.
PROMPT="[NoirVisor-LTP]$ "
PROMPT_PATTERN=re.escape(PROMPT)
# Password prompts of sudo ("[sudo] password for user: ") and sudo-rs ("[sudo: authenticate] Password: ").
SUDO_PROMPT_PATTERN=r"\[sudo[^\]\n]*\] [Pp]assword[^:\n]*: ?$"
# Escape sequences: OSC (e.g.: systemd's OSC 3008 context tags), CSI (e.g.: colors), and other two-character sequences.
ANSI_ESCAPE=re.compile(r"\x1b(\][^\x07\x1b]*(\x07|\x1b\\)|\[[0-9:;?<=>]*[ -/]*[@-~]|[@-Z\\-_])")
# An escape sequence that may continue in the next data.
ANSI_INCOMPLETE=re.compile(r"\x1b(\]((?!\x1b\\)[^\x07])*|\[[0-9:;?<=>]*[ -/]*)?$")

def usage():
	print("Usage: python run_ubuntu.py [options]")
	print("  -disk <path>         Disk image of the Ubuntu VM. (Default: {} in the current directory)".format(DEFAULT_DISK))
	print("  -ovmf-dir <path>     Directory of ovmf-code.fd and ovmf-vars.fd. (Default: the current directory)")
	print("  -accel <tcg|kvm>     Accelerator. (Default: tcg)")
	print("  -smp <count>         Number of processors. (Default: 1)")
	print("  -m <size>            Memory size. (Default: 8G)")
	print("  -release             Use the free build of NoirVisor instead of the checked build.")
	print("  -qemu <path>         Path to qemu-system-x86_64. (Default: found in PATH)")
	print("  -user <name>         User name to log in. (Default: {})".format(DEFAULT_USER))
	print("  -password <text>     Password of the user.")
	print("  -boot-timeout <sec>  Seconds to wait for the login prompt. (Default: 1800)")
	print("  -ltp-timeout <sec>   Seconds to wait for LTP to complete. (Default: 43200)")
	print("  -ltp-suites <names>  Space-separated LTP suites to run. (Default: \"{}\")".format(DEFAULT_LTP_SUITES))
	print("  -ltp-run <regex>     Run only the tests matching the regular expression. (Default: all tests)")
	print("  -ltp-skip <regex>    Skip the tests matching the regular expression. Pass \"\" to skip nothing.")
	print("                       (Default: \"{}\")".format(DEFAULT_LTP_SKIP))
	print("  -no-kernel-log-check Do not fail if the guest kernel logged warnings or got tainted.")
	print("  -log-dir <path>      Directory to keep serial and debug-console logs. (Default: a temporary directory)")
	print("  -quiet               Do not echo the serial console to stdout.")
	print("Exit codes: 0 if all tests passed, 1 if the VM failed (e.g.: boot, login or NoirVisor failure),")
	print("            2 if any test failed or broke, or the guest kernel logged warnings.")

class SerialConsole:
	"""Expect-like helper over QEMU's serial port, which is exposed as a TCP server."""
	def __init__(self,port,log_file,echo,abort_check):
		self.buffer=""
		self.pending=""
		self.decoder=codecs.getincrementaldecoder("utf-8")(errors="replace")
		self.log_file=log_file
		self.echo=echo
		self.abort_check=abort_check
		self.sock=None
		deadline=time.monotonic()+30
		while self.sock is None:
			try:
				self.sock=socket.create_connection(("127.0.0.1",port),timeout=1)
			except OSError:
				if time.monotonic()>deadline:
					raise
				time.sleep(0.2)
		self.sock.settimeout(1)

	def receive(self):
		try:
			data=self.sock.recv(65536)
		except socket.timeout:
			return
		if not data:
			raise EOFError("QEMU closed the serial port!")
		# A multi-byte UTF-8 character may be split across two receptions, so decode incrementally.
		text=self.decoder.decode(data)
		self.log_file.write(text)
		self.log_file.flush()
		if self.echo:
			sys.stdout.write(text)
			sys.stdout.flush()
		# Remove ANSI escape sequences so that the patterns are easy to write.
		# An escape sequence may be split across two receptions. Keep the incomplete part for the next reception.
		text=self.pending+text
		m=ANSI_INCOMPLETE.search(text)
		if m:
			self.pending=text[m.start():]
			text=text[:m.start()]
		else:
			self.pending=""
		self.buffer+=ANSI_ESCAPE.sub("",text).replace("\r","")

	def expect(self,patterns,timeout):
		"""Wait until one of the regular expressions matches. Return (index, text before the match, match)."""
		if isinstance(patterns,str):
			patterns=[patterns]
		compiled=[re.compile(p) for p in patterns]
		deadline=time.monotonic()+timeout
		while True:
			for i,p in enumerate(compiled):
				m=p.search(self.buffer)
				if m:
					before=self.buffer[:m.start()]
					self.buffer=self.buffer[m.end():]
					return i,before,m
			reason=self.abort_check()
			if reason is not None:
				raise RuntimeError(reason)
			if time.monotonic()>deadline:
				raise TimeoutError("Timed out waiting for {}!".format(patterns))
			self.receive()

	def send(self,text):
		self.sock.sendall(text.encode())

	def send_line(self,text):
		self.send(text+"\r")

	def close(self):
		self.sock.close()

def find_free_port():
	with socket.socket() as s:
		s.bind(("127.0.0.1",0))
		return s.getsockname()[1]

def run_command(console,command,timeout,password):
	"""Run a shell command in the guest and return its output. Answer sudo's password prompt if it asks."""
	console.send_line(command)
	output=""
	while True:
		i,before,_=console.expect([PROMPT_PATTERN,SUDO_PROMPT_PATTERN],timeout)
		output+=before
		if i==0:
			break
		console.send_line(password)
	# Drop the echoed command line.
	lines=output.split("\n")
	return "\n".join(lines[1:]) if len(lines)>1 else ""

def log_in(console,user,password):
	"""Log in at the login prompt, which must have been received already."""
	# The login prompt may appear before the boot finishes. While the guest is still busy (especially with TCG), login(1)
	# may give up after its own 60-second timeout, or the typed user name may be lost. Getty then prints a new login prompt.
	attempt=1
	while True:
		console.send_line(user)
		i,_,_=console.expect([r"Password: ?$",r"login: ?$"],300)
		if i==0:
			console.send_line(password)
			i,_,_=console.expect([r"[$#] ?$",r"Login incorrect",r"login: ?$"],300)
			if i==0:
				return
			reason="Login incorrect" if i==1 else "login timed out"
		else:
			reason="no password prompt"
		if attempt>=LOGIN_ATTEMPTS:
			raise RuntimeError("Failed to log in as {} after {} attempts ({})!".format(user,attempt,reason))
		print("\n[run_ubuntu] Login attempt {} failed ({}). Retrying...".format(attempt,reason))
		attempt+=1
		# "Login incorrect" is followed by a new login prompt.
		if reason=="Login incorrect":
			console.expect(r"login: ?$",300)

def extract_summary(output):
	"""Extract kirk's final summary block, which follows the results of the last suite."""
	lines=[l.rstrip() for l in output.split("\n")]
	# kirk ends with a "TEST SUMMARY" block covering all suites, followed by "Session stopped".
	start=None
	for i,line in enumerate(lines):
		if re.match(r"\s*TEST SUMMARY\b",line):
			start=i
	if start is None:
		# Fallback: the last 30 lines.
		start=max(0,len(lines)-30)
	summary=[l for l in lines[start:] if l and l!="Session stopped"]
	return "\n".join(summary)

def check_kernel(console,password):
	"""Check the guest kernel's log and taint flags. Return a list of problems, which is empty if nothing is wrong."""
	# kirk only counts test results. Hypervisor bugs often show up as kernel warnings while the tests still pass.
	problems=[]
	taint_text=run_command(console,"cat /proc/sys/kernel/tainted",60,password).strip()
	try:
		taint=int(taint_text)
	except ValueError:
		return ["Failed to read the taint flags of the kernel (\"{}\").".format(taint_text)]
	for bit,flag,description in BAD_TAINT_FLAGS:
		if taint&(1<<bit):
			problems.append("The kernel is tainted with {} ({}).".format(flag,description))
	# Only the messages with warning or higher level are needed.
	# Read them from the journal rather than the kernel's ring buffer: some tests (e.g.: kmsg01) clear the ring buffer.
	log=run_command(console,"sudo journalctl -k -b -p warning --no-pager -o short-monotonic",120,password)
	lines=[l.rstrip() for l in log.split("\n") if KERNEL_LOG_PATTERN.search(l)]
	if lines:
		problems.append("The kernel logged {} suspicious message(s):".format(len(lines)))
		# Too many messages are not helpful. The first ones are the most important.
		problems+=["    "+l for l in lines[:20]]
		if len(lines)>20:
			problems.append("    ...")
	return problems

def check_results(output):
	"""Check kirk's final summary. Return a list of problems, which is empty if all tests passed."""
	# kirk exits with zero even if tests fail, so the counts in the summary must be checked.
	m=None
	for m in re.finditer(r"^\s*TEST SUMMARY\b",output,re.MULTILINE):
		pass
	if m is None:
		return ["kirk did not print the test summary."]
	summary=output[m.end():]
	problems=[]
	for name in ["Failed","Broken"]:
		count=re.search(r"^\s*{}:\s*(\d+)\s*$".format(name),summary,re.MULTILINE)
		if count is None:
			problems.append("The \"{}\" count is missing from the test summary.".format(name))
		elif int(count.group(1))>0:
			problems.append("{} {} test(s).".format(count.group(1),name.lower()))
	return problems

if __name__=="__main__":
	# The guest prints characters (e.g.: box-drawing characters) that the console's code page may not encode.
	sys.stdout.reconfigure(errors="replace")
	accel_name="tcg"
	smp_count=1
	ram="8G"
	build_preset="chk"
	qemu="qemu-system-x86_64"
	# The disk image and UEFI firmware files are in the current directory by default.
	disk_path=os.path.abspath(DEFAULT_DISK)
	ovmf_dir=os.getcwd()
	user=DEFAULT_USER
	password=DEFAULT_PASSWORD
	boot_timeout=1800
	ltp_timeout=43200
	ltp_suites=DEFAULT_LTP_SUITES
	ltp_run=None
	ltp_skip=DEFAULT_LTP_SKIP
	log_dir=None
	echo=True
	kernel_log_check=True
	i=1
	while i<len(sys.argv):
		arg=sys.argv[i]
		if arg=="-accel":
			i+=1
			accel_name=sys.argv[i]
		elif arg=="-smp":
			i+=1
			smp_count=int(sys.argv[i])
		elif arg=="-m":
			i+=1
			ram=sys.argv[i]
		elif arg=="-release":
			build_preset="fre"
		elif arg=="-qemu":
			i+=1
			qemu=sys.argv[i]
		elif arg=="-disk":
			i+=1
			disk_path=os.path.abspath(sys.argv[i])
		elif arg=="-ovmf-dir":
			i+=1
			ovmf_dir=os.path.abspath(sys.argv[i])
		elif arg=="-user":
			i+=1
			user=sys.argv[i]
		elif arg=="-password":
			i+=1
			password=sys.argv[i]
		elif arg=="-boot-timeout":
			i+=1
			boot_timeout=int(sys.argv[i])
		elif arg=="-ltp-timeout":
			i+=1
			ltp_timeout=int(sys.argv[i])
		elif arg=="-ltp-suites":
			i+=1
			ltp_suites=sys.argv[i]
		elif arg=="-ltp-run":
			i+=1
			ltp_run=sys.argv[i]
		elif arg=="-ltp-skip":
			i+=1
			ltp_skip=sys.argv[i]
		elif arg=="-log-dir":
			i+=1
			log_dir=sys.argv[i]
		elif arg=="-quiet":
			echo=False
		elif arg=="-no-kernel-log-check":
			kernel_log_check=False
		elif arg in ["-h","-help","--help"]:
			usage()
			exit(0)
		else:
			print("Unknown argument: {}!".format(arg))
			usage()
			exit(1)
		i+=1
	# Validate the patterns here rather than failing in the guest after a long boot.
	for pattern in [ltp_run,ltp_skip]:
		if pattern:
			try:
				re.compile(pattern)
			except re.error as e:
				print("Invalid regular expression \"{}\": {}".format(pattern,e))
				exit(1)
	ltp_command="sudo ./kirk -f {}".format(" ".join(shlex.quote(s) for s in ltp_suites.split()))
	if ltp_run:
		ltp_command+=" -p {}".format(shlex.quote(ltp_run))
	if ltp_skip:
		ltp_command+=" -s {}".format(shlex.quote(ltp_skip))
	if accel_name=="tcg":
		accel_arg="tcg,thread=single"
		cpu_arg="max,hypervisor=off,svm=on"
		machine_arg="q35,smm=on,kernel-irqchip=off"
	elif accel_name=="kvm":
		accel_arg="kvm"
		cpu_arg="host,hypervisor=off"
		machine_arg="q35,smm=on,kernel-irqchip=split"
	else:
		print("Unknown accelerator name \"{}\"!".format(accel_name))
		exit(1)
	script_dir=os.path.dirname(os.path.abspath(__file__))
	noirvisor_image=os.path.join(script_dir,"..","bin","comp{}_uefix64".format(build_preset),"NoirVisor-Uefi.img")
	for f in [noirvisor_image,disk_path,os.path.join(ovmf_dir,"ovmf-code.fd"),os.path.join(ovmf_dir,"ovmf-vars.fd")]:
		if not os.path.isfile(f):
			print("Error: {} is not found!".format(f))
			exit(1)
	temp_dir=tempfile.mkdtemp(prefix="noirvisor-ubuntu-")
	if log_dir is None:
		log_dir=temp_dir
	os.makedirs(log_dir,exist_ok=True)
	# Use a copy of the UEFI variable store so that the VM's own copy stays untouched.
	vars_copy=os.path.join(temp_dir,"ovmf-vars.fd")
	shutil.copyfile(os.path.join(ovmf_dir,"ovmf-vars.fd"),vars_copy)
	serial_log_path=os.path.join(log_dir,"serial.log")
	debugcon_log_path=os.path.join(log_dir,"debugcon.log")
	port=find_free_port()
	cmd_list=[
		qemu,
		"-accel",accel_arg,
		"-machine",machine_arg,
		"-cpu",cpu_arg,
		"-smp",str(smp_count),
		"-m",ram,
		"-drive","format=raw,file={},snapshot=on".format(noirvisor_image),
		# The disk is never written: all writes go to a temporary overlay that is discarded when QEMU exits.
		"-drive","file={},snapshot=on".format(disk_path),
		"-nic","model=e1000e",
		"-drive","if=pflash,format=raw,unit=0,readonly=on,file={}".format(os.path.join(ovmf_dir,"ovmf-code.fd")),
		"-drive","if=pflash,format=raw,unit=1,file={}".format(vars_copy),
		"-display","none",
		"-chardev","socket,id=serial0,host=127.0.0.1,port={},server=on,wait=on".format(port),
		"-serial","chardev:serial0",
		"-chardev","file,id=debugger,path={}".format(debugcon_log_path),
		"-device","isa-debugcon,iobase=0xE9,chardev=debugger",
		"-monitor","none"]
	print(cmd_list)
	print("Serial log: {}".format(serial_log_path))
	print("Debug-console log: {}".format(debugcon_log_path))
	qemu_process=subprocess.Popen(cmd_list)
	def abort_check():
		if qemu_process.poll() is not None:
			return "QEMU exited unexpectedly with code {}!".format(qemu_process.returncode)
		try:
			with open(debugcon_log_path,"r",errors="replace") as f:
				dcon=f.read()
		except OSError:
			return None
		for marker in ["panic","Host is not expecting"]:
			if marker in dcon:
				return "NoirVisor reported a failure (\"{}\")! See {}.".format(marker,debugcon_log_path)
		return None
	succeeded=False
	summary=None
	ltp_problems=None
	kernel_problems=None
	serial_log=open(serial_log_path,"w",encoding="utf-8",errors="replace")
	console=None
	try:
		console=SerialConsole(port,serial_log,echo,abort_check)
		start=time.monotonic()
		# NoirVisor's prompts and boot selection time out on their own.
		# GRUB's menu is answered with Enter so that it does not wait.
		login_deadline=start+boot_timeout
		while True:
			i,_,_=console.expect([r"Press enter to boot the selected OS|Use the \^ and v keys",r"login: ?$"],login_deadline-time.monotonic())
			if i==0:
				console.send("\r")
			else:
				break
		print("\n[run_ubuntu] Login prompt appeared after {:.0f} seconds.".format(time.monotonic()-start))
		log_in(console,user,password)
		# Set a distinctive prompt. Disable the pager and keep long lines intact.
		# The prompt is split by quotes, so that the echoed command itself does not match the prompt.
		console.send_line("export PS1='{}''{}' PAGER=cat SYSTEMD_PAGER=cat; stty cols 250".format(PROMPT[:5],PROMPT[5:]))
		console.expect(PROMPT_PATTERN,60)
		print("\n[run_ubuntu] Logged in. Kernel: {}".format(run_command(console,"uname -r",60,password).strip()))
		run_command(console,"cd {}".format(LTP_DIR),60,password)
		# Write the "noirvisor" suite. Each line of a runtest file is a test name followed by its command.
		suite_lines=" ".join(shlex.quote(l) for l in NOIRVISOR_SUITE)
		run_command(console,"printf '%s\\n' {} | sudo tee runtest/noirvisor >/dev/null".format(suite_lines),60,password)
		print("[run_ubuntu] Running LTP: {}".format(ltp_command))
		ltp_start=time.monotonic()
		ltp_output=run_command(console,ltp_command,ltp_timeout,password)
		print("\n[run_ubuntu] LTP completed in {:.0f} seconds.".format(time.monotonic()-ltp_start))
		summary=extract_summary(ltp_output)
		ltp_problems=check_results(ltp_output)
		# kirk exits with non-zero code if the session itself fails (e.g.: a suite is not found).
		kirk_rc=run_command(console,"echo $?",60,password).strip()
		if kirk_rc!="0":
			ltp_problems.append("kirk exited with code {}.".format(kirk_rc))
		if kernel_log_check:
			kernel_problems=check_kernel(console,password)
		# Shut down the VM.
		console.send_line("sudo poweroff")
		try:
			i,_,_=console.expect([SUDO_PROMPT_PATTERN,r"reboot: Power down"],120)
			if i==0:
				console.send_line(password)
		except (TimeoutError,EOFError,RuntimeError):
			pass
		# Keep draining the serial port so that the shutdown messages are logged.
		poweroff_deadline=time.monotonic()+300
		while qemu_process.poll() is None and time.monotonic()<poweroff_deadline:
			try:
				console.receive()
			except (EOFError,OSError):
				break
		try:
			qemu_process.wait(timeout=max(0,poweroff_deadline-time.monotonic()))
		except subprocess.TimeoutExpired:
			print("[run_ubuntu] The VM did not power off in time!")
		succeeded=True
	except (TimeoutError,EOFError,RuntimeError,OSError) as e:
		print("\n[run_ubuntu] Error: {}".format(e))
	finally:
		if console is not None:
			console.close()
		if qemu_process.poll() is None:
			qemu_process.kill()
			qemu_process.wait()
		serial_log.close()
		if log_dir!=temp_dir:
			shutil.rmtree(temp_dir,ignore_errors=True)
	if summary is not None:
		print("\n========== LTP Summary ==========")
		print(summary)
		print("=================================")
	if ltp_problems:
		print("[run_ubuntu] LTP failed:")
		for p in ltp_problems:
			print("  - {}".format(p))
	elif ltp_problems is not None:
		print("[run_ubuntu] LTP passed.")
	if kernel_problems:
		print("[run_ubuntu] Kernel check failed:")
		for p in kernel_problems:
			print("  - {}".format(p) if not p.startswith(" ") else p)
	elif kernel_problems is not None:
		print("[run_ubuntu] Kernel check passed.")
	print("Logs are kept in {}.".format(log_dir))
	if not succeeded:
		exit(1)
	exit(2 if ltp_problems or kernel_problems else 0)
