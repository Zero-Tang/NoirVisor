import os
import socket
import subprocess
import sys
import threading

if __name__=="__main__":
	# OVMF must be present.
	subprocess.call([sys.executable,"download-ovmf.py"])
	accel_name="tcg"
	debug_log=None
	can_run=True
	ram="128M"
	cpu_arg="{},hypervisor=off"
	machine_arg="q35,smm=on,{}"
	smp_count=1
	gdb=False
	build_preset="chk"
	iommu=None
	iommu_arg=None
	pcileech=False
	debugcon=0xE9
	debugcon_chardev="stdio"
	monitor=None
	nographic=False
	auto_check=False
	# Check command-line arguments
	i=1
	while i<len(sys.argv):
		if sys.argv[i]=="-accel":
			i+=1
			accel_name=sys.argv[i]
		elif sys.argv[i]=="-debug":
			i+=1
			debug_log=sys.argv[i]
		elif sys.argv[i]=="-smp":
			i+=1
			smp_count=int(sys.argv[i])
		elif sys.argv[i]=="-m":
			i+=1
			ram=sys.argv[i]
		elif sys.argv[i]=="-gdb":
			gdb=True
		elif sys.argv[i]=="-release":
			build_preset="fre"
		elif sys.argv[i]=="-iommu":
			i+=1
			iommu=sys.argv[i].lower()
		elif sys.argv[i]=="-pcileech":
			pcileech=True
		elif sys.argv[i]=="-debugcon":
			i+=1
			debugcon=eval(sys.argv[i])
			if not isinstance(debugcon,int):
				can_run=False
				print("Error: ISA-DebugCon I/O Port number must be an integer!")
		elif sys.argv[i]=="-dcon-dev":
			i+=1
			debugcon_chardev=sys.argv[i]
		elif sys.argv[i]=="-nographic":
			nographic=True
		elif sys.argv[i]=="-monitor":
			i+=1
			monitor=int(sys.argv[i])
		elif sys.argv[i]=="-auto-check":
			auto_check=True
		else:
			print("Unknown argument: {}!".format(sys.argv[i]))
		i+=1
	if auto_check:
		PORT=12345
		# If auto-check is enabled, several arguments are fixed.
		nographic=True
		debugcon_chardev="socket,port={},host=127.0.0.1,server=on".format(PORT)
	if iommu=="intel":
		iommu_arg="intel-iommu,aw-bits=48"
	elif iommu=="amd":
		iommu_arg="amd-iommu,xtsup=on,dma-remap=on"
	elif iommu is None:
		iommu_arg=None
	else:
		can_run=False
		print("Error: Unknown IOMMU vendor: {}! Must be either intel or amd!".format(iommu))
	if accel_name=="tcg":
		cpu_arg=cpu_arg.format("max")
		machine_arg=machine_arg.format("accel=tcg,kernel-irqchip=off")
	elif accel_name=="whpx":
		can_run=False
		print("Error: WHPX accelerator does not support nested virtualization!")
	elif accel_name=="kvm":
		# Check KVM availability.
		if os.path.exists("/dev/kvm"):
			# Check vendor ID to make sure whether we're using vmx or svm.
			f=open("/proc/cpuinfo",'r')
			L=f.readlines()
			f.close()
			for s in L:
				if s.startswith("vendor_id"):
					tmp=s.split(':')
					vendor_name=tmp[1].strip()
					# Intel, VIA, Centaur and Zhaoxin support Intel VT-x.
					if vendor_name=="GenuineIntel" or vendor_name=="  Shanghai  "or vendor_name=="VIA VIA VIA " or vendor_name=="CentaurHauls":
						cpu_arg=cpu_arg.format("host,vmx=on")
					# AMD and Hygon support AMD-V.
					elif vendor_name=="AuthenticAMD" or vendor_name=="HygonGenuine":
						cpu_arg=cpu_arg.format("host,svm=on")
					else:
						can_run=False
						print("This CPU's Vendor ID ({}) is unknown!".format(vendor_name))
					break
			machine_arg=machine_arg.format("accel=kvm,kernel-irqchip=split")
		else:
			can_run=False
			print("Error: KVM is absent!")
	else:
		can_run=False
		print("Unknown accelerator name \"{}\"!".format(accel_name))
	if can_run:
		cmd_list=[
			"qemu-system-x86_64",
			"-machine",machine_arg,
			"-cpu",cpu_arg,
			"-smp",str(smp_count),
			"-m",ram,
			"-drive","if=pflash,format=raw,unit=0,readonly=on,file=ovmf-code.fd",
			"-drive","if=pflash,format=raw,unit=1,readonly=on,file=ovmf-vars.fd",
			"-drive","format=raw,file="+os.path.join("..","bin","comp{}_uefix64".format(build_preset),"NoirVisor-Uefi.img"),
			"-chardev","{},id=debugger".format(debugcon_chardev),
			"-device","isa-debugcon,iobase=0x{:X},chardev=debugger".format(debugcon)]
		if not debug_log is None:
			cmd_list+=["-d",debug_log,"-D","qemu.log"]
		if nographic:
			cmd_list.append("-nographic")
		if gdb:
			cmd_list.append("-s")
		if not iommu is None:
			cmd_list+=["-device",iommu_arg]
		if pcileech:
			cmd_list+=["-chardev","socket,id=pcileech,wait=off,server=on,host=0.0.0.0,port=6789","-device","pcileech,chardev=pcileech"]
		if not monitor is None:
			cmd_list+=["-monitor","telnet:0.0.0.0:{},server=on".format(monitor)]
		print(cmd_list)
		if auto_check:
			# Auto-check is configured.
			# Redirect the serial and debugcon.
			debugcon_output=[]
			def receive_debugcon():
				while qemu_process.poll() is None:
					try:
						with socket.create_connection(("127.0.0.1",PORT),timeout=0.1) as connection:
							connection.settimeout(None)
							while True:
								data=connection.recv(4096)
								if not data:
									break
								debugcon_output.append(data)
						return
					except (ConnectionRefusedError, TimeoutError):
						continue
			qemu_process=subprocess.Popen(cmd_list,stdout=subprocess.PIPE)
			debugcon_thread=threading.Thread(target=receive_debugcon)
			debugcon_thread.start()
			serial,_=qemu_process.communicate()
			debugcon_thread.join()
			sys.stdout.flush()
			serial=serial.decode(errors="replace")
			debugcon=b"".join(debugcon_output).decode(errors="replace")
			sys.stdout.write(serial)
			sys.stdout.write(debugcon)
			# Check the outputs.
			# Check if the system subversion completes. Also checks the CI-fault test.
			i=debugcon.find("System subversion completed!")
			assert i!=-1
			assert debugcon.find("CI-fault",i)!=-1
			if not iommu is None:
				# IOMMU is enabled. Check if MBR signature word is zeroed.
				i=serial.find("MBR Signature Word: 0x0000")
				assert i!=-1
			print("QEMU Test Passed!")
		else:
			subprocess.call(cmd_list)
	exit(0)