import json
import struct
import sys

class record:
	def __init__(self,name:str,value):
		self.name:str=name
		self.value=value
	
	# Implement these comparison methods to help sorting
	def __lt__(self,comp):
		return self.name<comp.name
	
	def __gt__(self,comp):
		return self.name>comp.name
	
	def __le__(self,comp):
		return self.name<=comp.name
	
	def __ge__(self,comp):
		return self.name>=comp.name
	
	def __eq__(self,comp):
		return self.name==comp.name
	
	def __str__(self):
		return "{}: {} {}\n".format(self.name,self.value,type(self.value))

	# Generate the record in binary format
	def output_binary(self)->bytes:
		name_bin:bytes=self.name.encode('latin-1')+b'\0'
		value_bin:bytes=None
		value_type:int=None
		if type(self.value)==str:
			value_bin=self.value.encode('latin-1')+b'\0'
			value_type=0
		elif type(self.value)==int:
			value_bin=struct.pack('<I',self.value)
			value_type=1
		elif type(self.value)==bool:
			value_bin=b'\x01' if self.value else b'\x00'
			value_type=2
		else:
			print("Type {} is unsupported!".format(type(self.value)))
		raw_bin:bytes=struct.pack('<I',value_type)
		raw_bin+=struct.pack('<I',len(name_bin))
		raw_bin+=struct.pack('<I',len(value_bin))
		# Pad the strings to alignment
		raw_bin+=name_bin
		if len(raw_bin)&3:
			raw_bin+=(4-(len(name_bin)&3))*b'\0'
		raw_bin+=value_bin
		if len(value_bin)&3:
			raw_bin+=(4-(len(value_bin)&3))*b'\0'
		return raw_bin

if __name__=="__main__":
	if len(sys.argv)<3:
		print("Insufficient Argument!")
	else:
		source:str=sys.argv[1]
		destination:str=sys.argv[2]
		# Load the configuration file in json text.
		f=open(source,'r')
		d=json.loads(f.read())
		f.close()
		# We need a sorted list to help binary search.
		l:list[record]=[]
		for r in d:
			l.append(record(r,d[r]))
		l.sort()
		# Generate all records in binary format.
		out_list:list[bytes]=[]
		for r in l:
			out_list.append(r.output_binary())
		# Construct header
		head_bin:bytes=struct.pack("<I",len(out_list))
		pos=4+len(out_list)*4
		for o in out_list:
			head_bin+=struct.pack("<I",pos)
			pos+=len(o)
		f=open(destination,"wb")
		# Write all binary records
		f.write(head_bin)
		for o in out_list:
			f.write(o)
		f.close()