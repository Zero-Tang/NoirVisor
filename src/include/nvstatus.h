/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

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
	Other values are reserved for future usage.

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
*/

#define noir_hypervision_absent			0xC0000005

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