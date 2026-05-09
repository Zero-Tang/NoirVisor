import os
import subprocess
import sys

if __name__=="__main__":
	# OVMF must be present.
	subprocess.call([sys.executable,"download-ovmf.py"])
	accel_name="tcg"
	debug_log=None
	can_run=True
	cpu_arg="{},hypervisor=off"
	machine_arg="pc,smm=on,{}"
	smp_count=1
	gdb=False
	build_preset="chk"
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
		elif sys.argv[i]=="-gdb":
			gdb=True
		elif sys.argv[i]=="-release":
			build_preset="fre"
		else:
			print("Unknown argument: {}!".format(sys.argv[i]))
		i+=1
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
			"-drive","if=pflash,format=raw,unit=0,readonly=on,file=ovmf-code.fd",
			"-drive","if=pflash,format=raw,unit=1,readonly=on,file=ovmf-vars.fd",
			"-drive","format=raw,file="+os.path.join("..","bin","comp{}_uefix64".format(build_preset),"NoirVisor-Uefi.img"),
			"-debugcon","stdio"]
		if not debug_log is None:
			cmd_list+=["-d",debug_log,"-D","qemu.log"]
		if gdb:
			cmd_list.append("-s")
		print(cmd_list)
		subprocess.call(cmd_list)