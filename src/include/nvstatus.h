/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file defines different status indicator for NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/noirhvm.h
*/

/*
  Introduction to NV-Status layout:
  NV-Status is a 32-bit value and are consisted of following parts:

  S - Severity Code (2 bits, bits 30-31)
	00 - Success
	01 - Info
	02 - Warning
	03 - Error

  F - Facility Core (6 bits, bits 24-29)
	00 - Cross-Platform or General Facility (xpf_core)
	01 - Intel VT-x Specific Facility (vt_core)
	02 - AMD-V Specific Facility (svm_core)
	03 - Instruction Emulator
	Other values (03-63) are reserved for future usage.

  C - Detailed Code (24 bits, bits 0-24)

  To visualize the NV-Status, we have the following layout graph:
  31	30			24												0
  +-----+-----------+-----------------------------------------------+
  |  S  |	  F		| Detailed Code									|
  +-----+-----------+-----------------------------------------------+
*/

typedef u32 noir_status;

// Macros for defining status.
#define noir_status_severity(st)		(st>>30)
#define noir_status_facility(st)		((st>>24)&0x3f)
#define noir_status_code(st)			(st&0xffffff)

/*
  Status Indicator: noir_success
  If a procedure is executed successfully,
  this value is supposed to be returned.

  Value: 0x0
*/

#define noir_success					0x0

/*
  Status Indicator: noir_already_rescinded
  If execution of a vCPU is already rescinded,
  this value is supposed to be returned.

  Value: 0x40000001
*/

#define noir_already_rescinded			0x40000001

/*
  Status Indicator: noir_dereference_destroying
  If an object is dereferenced to the point it is being
  released, this value is supposed to be returned.

  Value: 0x40000002
*/

#define noir_dereference_destroying		0x40000002

/*
  Status Indicator: noir_emu_dual_memory_operands
  If an instruction operand has two memory operands,
  this value is supposed to be returned so that the
  user hypervisor should do further processing.
  (Including address translations and access checks)

  Value: 0x43000000
*/

#define noir_emu_dual_memory_operands	0x43000000

/*
  Status Indicator noir_unsuccessful
  If a procedure failed to execute due to unknown error,
  this value is supposed to be returned.

  Value: 0xC0000000
*/

#define noir_unsuccessful				0xC0000000

/*
  Status Indicator: noir_insufficient_resources
  If a procedure encounters lack of resource,
  this value is supposed to be returned.

  Value: 0xC0000001
*/

#define noir_insufficient_resources		0xC0000001

/*
  Status Indicator: noir_not_implemented
  If a function is not yet implemented,
  this value is supposed to be returned.

  Value: 0xC0000002
*/

#define noir_not_implemented			0xC0000002

/*
  Status Indicator: noir_unknown_processor
  If NoirVisor's core functionality detects
  an unknown processor, then
  this value is supposed to be returned.

  Value: 0xC0000003
*/

#define noir_unknown_processor			0xC0000003

/*
  Status Indicator: noir_invalid_parameter
  If invalid parameters are passed to the function,
  then this value is supposed to be returned.

  Value: 0xC0000004
*/

#define noir_invalid_parameter			0xC0000004

/*
  Status Indicator: noir_hypervision_absent
  If the function requires hypervisor to be present
  in the system and hypervisor is actually absent,
  then this value is supposed to be returned.

  Value: 0xC0000005
*/

#define noir_hypervision_absent			0xC0000005

/*
  Status Indicator: noir_vcpu_already_created
  If an attempt to create a vCPU with identifier
  that is already created within a Customizable VM,
  then this value is supposed to be returned.

  Value: 0xC0000006
*/

#define noir_vcpu_already_created		0xC0000006

/*
  Status Indicator: noir_buffer_too_small
  If the buffer passed to the function is too small,
  then this value is supposed to be returned.

  Value: 0xC0000007
*/

#define noir_buffer_too_small			0xC0000007

/*
  Status Indicator: noir_vcpu_not_exist
  If a specified vCPU does not exist, then
  this value is supposed to be returned.

  Value: 0xC0000008
*/

#define noir_vcpu_not_exist				0xC0000008

/*
  Status Indicator: noir_user_page_violation
  If the page specified for CVM address
  mapping does not meet the requirements,
  then this value is supposed to be returned.

  Value: 0xC0000009
*/

#define noir_user_page_violation		0xC0000009

/*
  Status Indicator: noir_guest_page_absent
  If a guest page to be accessed by
  the hypervisor does not exist,
  then this value is supposed to be returned.

  Value: 0xC000000A
*/

#define noir_guest_page_absent			0xC000000A

/*
  Status Indicator: noir_access_denied
  If a procedure is accessing something protected,
  then this value is supposed to be returned.

  Value: 0xC000000B
*/

#define noir_access_denied				0xC000000B

/*
  Status Indicator: noir_hardware_error
  If a procedure encounters an external hardware
  error, then this value is supposed to be returned.

  Value: 0xC000000C
*/

#define noir_hardware_error				0xC000000C

/*
  Status Indicator: noir_uninitialized
  If a procedure is accessing some uninitialized data,
  then this value is supposed to be returned.

  Value: 0xC000000D
*/

#define noir_uninitialized				0xC000000D

/*
  Status Indicator: noir_nsv_violation
  If a certain operation is violating NSV policies,
  then this value is supposed to be returned.

  Value: 0xC000000E
*/

#define noir_nsv_violation				0xC000000E

/*
  Status Indicator: noir_acpi_no_such_table
  If the specific ACPI table is not defined in current system,
  then this value is supposed to be returned.

  Value: 0xC000000F
*/

#define noir_acpi_no_such_table			0xC000000F

/*
  Status Indicator: noir_not_intel
  If a procedure is specific for Intel Processor,
  and the processor is not manufactured by Intel,
  this value is supposed to be returned.
  This indicator is from Intel VT-x Specific Facility.
  
  Value: 0xC1000000
*/

#define noir_not_intel					0xC1000000

/*
  Status Indicator: noir_vmx_not_supported
  If Intel VT-x is not supported,
  this value is supposed to be returned.
  This indicator is from Intel VT-x Specific Facility.

  Value: 0xC1000001
*/

#define noir_vmx_not_supported			0xC1000001

/*
  Status Indicator: noir_ept_not_supported
  If Intel EPT (Extended Page Table) is not supported,
  this value is supposed to be returned.
  This indicator is from Intel VT-x Specific Facility.

  Value: 0xC1000002
*/

#define noir_ept_not_supported			0xC1000002

/*
  Status Indicator: noir_not_amd
  If a procedure is specific for AMD Processor,
  and the processor is not manufactured by AMD,
  this value is supposed to be returned.
  This indicator is from AMD-V Specific Facility.

  Value: 0xC2000000
*/

#define noir_not_amd					0xC2000000

/*
  Status Indicator: noir_svm_not_supported
  If AMD-V is not supported,
  this value is supposed to be returned.
  This indicator is from AMD-V Specific Facility.

  Value: 0xC2000001
*/

#define noir_svm_not_supported			0xC2000001

/*
  Status Indicator: noir_npt_not_supported
  If Nested Paging is not supported,
  this value is supposed to be returned.
  This indicator is from AMD-V Specific Facility.

  Value: 0xC2000002
*/

#define noir_npt_not_supported			0xC2000002

/*
  Status Indicator: noir_emu_non_emulatable
  If the VM-Exit is not emulatable,
  then this value is supposed to be returned.
  This indicator is from Instruction Emulator.

  Value: 0xC3000000
*/

#define noir_emu_not_emulatable			0xC3000000

/*
  Status Indicator: noir_emu_unknown_instruction
  If the instruction emulated for MMIO is unrecognized by
  the emulator, then this value is supposed to be returned.
  This indicator is from Instruction Emulator.

  Value: 0xC3000001
*/

#define noir_emu_unknown_instruction	0xC3000001