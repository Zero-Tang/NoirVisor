import os
import subprocess
import sys

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
	monitor=None
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
		elif sys.argv[i]=="-monitor":
			i+=1
			monitor=int(sys.argv[i])
		else:
			print("Unknown argument: {}!".format(sys.argv[i]))
		i+=1
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
			"-chardev","stdio,id=debugger",
			"-device","isa-debugcon,iobase=0x{:X},chardev=debugger".format(debugcon)]
		if not debug_log is None:
			cmd_list+=["-d",debug_log,"-D","qemu.log"]
		if gdb:
			cmd_list.append("-s")
		if not iommu is None:
			cmd_list+=["-device",iommu_arg]
		if pcileech:
			cmd_list+=["-chardev","socket,id=pcileech,wait=off,server=on,host=0.0.0.0,port=6789","-device","pcileech,chardev=pcileech"]
		if not monitor is None:
			cmd_list+=["-monitor","telnet:0.0.0.0:{},server=on".format(monitor)]
		print(cmd_list)
		subprocess.call(cmd_list)