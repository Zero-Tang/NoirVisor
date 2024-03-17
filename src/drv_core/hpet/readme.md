# High-Precision Event Timer Driver
NoirVisor may need to access timer hardware to get to real time instead of processor TSC.

# HPET Specification
Here lists some definitions of HPET Hardware.

## Location of HPET I/O
HPET is accessed through MMIO (Memory-Mapped I/O). The base address of MMIO is defined via ACPI HPET Table. Each timer hardware takes 1024 bytes of MMIO range.