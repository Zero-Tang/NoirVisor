#!/usr/bin/python3
import os
import platform
import shutil
import subprocess
import sys
import time

from pipeline import Pipeline

def confirm_cargo()->bool:
	proc=subprocess.Popen(["cargo","--version"],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
	try:
		stdout_bytes,stderr_bytes=proc.communicate()
		stdout_text=stdout_bytes.decode()
		if not stdout_text.startswith("cargo"):
			print("This cargo doesn't seem right...")
			return False
		cargo_ver=stdout_text[:-1]
	except:
		print("Cargo does not exist! Did you install rust-lang?")
		return False
	proc=subprocess.Popen(["rustc","--version"],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
	try:
		stdout_bytes,stderr_bytes=proc.communicate()
		stdout_text=stdout_bytes.decode()
		if not stdout_text.startswith("rustc"):
			print("This rustc doesn't seem right...")
			return False
		rustc_ver=stdout_text[:-1]
	except:
		print("rustc does not exist! Did you install rust-lang?")
		return False
	print("We are using {} and {}...".format(cargo_ver,rustc_ver))
	return True

def do_build():
	i=1
	optimizer_enabled=False
	target="uefi"
	# NoirVisor usually doesn't use more than 20KB of heap.
	# 256KiB of heap granularity should be quite enough.
	os.environ["DEFAULT_MMAP_GRANULARITY"]="0x40000"
	while i<len(sys.argv):
		if sys.argv[i]=="/target":
			i+=1
			config="build-{}.json".format(sys.argv[i])
			target=sys.argv[i]
		elif sys.argv[i].startswith("/opt:"):
			expr=sys.argv[i][5:]
			if expr.lower()=="yes" or expr.lower()=="true":
				optimizer_enabled=True
			elif expr.lower()=="no" or expr.lower()=="false":
				optimizer_enabled=False
			else:
				print("Ignoring unknown optimization argument {}!".format(sys.argv[i]))
		else:
			print("Ignoring unknown argument {}!".format(sys.argv[i]))
		i+=1
	config="build-{}.json".format(target)
	if confirm_cargo():
		extra_vars_dict={"python":sys.executable}
		pl=Pipeline(config,optimizer_enabled,extra_vars=extra_vars_dict)
		return pl.run()
	else:
		return 100

def do_check():
	old_ev=os.environ
	os.environ["AR_x86_64-unknown-uefi"]="llvm-ar"
	os.environ["CC_x86_64-unknown-uefi"]="clang-cl"
	os.environ["CFLAGS_x86_64-unknown-uefi"]="/GS- --target=x86_64-pc-windows-msvc -mno-sse -gcodeview"
	subprocess.run(["cargo","audit"])
	subprocess.run(["cargo","clippy","--target","x86_64-unknown-uefi","--package","nvcore","--package","loadefi","--package","cvsched"])
	subprocess.run(["cargo","clippy","--target","x86_64-pc-windows-msvc","--package","cvsched"])
	subprocess.run(["cargo","clippy","--package","cvmock","--all-targets"])
	subprocess.run(["cargo","fmt","--check"])
	os.environ=old_ev

def do_clean():
	subprocess.run(["cargo","clean"])
	shutil.rmtree("bin")

def do_test():
	subprocess.run(["cargo","test","--package","cvsched","--package","htable"])

def main():
	action="build"
	known_actions={"build","check","clean","test"}
	if len(sys.argv)>1 and sys.argv[1] in known_actions:
		action=sys.argv[1]
	# Dispatch the action.
	action_table={"build":do_build,"check":do_check,"clean":do_clean,"test":do_test}
	return action_table[action]()

if __name__=="__main__":
	known_platforms={"Windows","Linux"}
	if not platform.system() in known_platforms:
		print("Host OS {} is unsupported by this build script!".format(platform.system()))
		exit()
	t1:float=time.time()
	code=main()
	t2:float=time.time()
	print("{} seconds spent in compilation!".format(t2-t1))
	exit(code)
