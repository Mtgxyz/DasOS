#include "console.hpp"
#include "pmm.hpp"
#include "numeric.hpp"
#include "pointer.hpp"
#include "multiboot.hpp"
#include "gdt.hpp"
#include "idt.hpp"
#include "compat.h"
#include "io.hpp"
#include "vmm.hpp"
#include "elf.hpp"
#include "bsod.hpp"

#include "asm.hpp"

#include "driver/timer.hpp"
#include "driver/keyboard.hpp"

#include <inttypes.h>
#include <new>

#include <string.h>

namespace vm {
#include <vm.h>
}

using namespace multiboot;
using namespace console_tools;

struct dummy;

// Symbols generated by linker, no useful content in there...
extern dummy kernelStartMarker;
extern dummy kernelEndMarker;

driver::Timer timer;
driver::Keyboard keyboardDriver;

VMMContext * kernelContext;

/*
static const uint32_t entryPointAddress = 0x40000000;

void run_program0(Module const & module)
{
	using namespace elf;
	
	const Header *header = module.start.data<elf::Header>();
	
	ProgramHeader *ph;
	int i;
	if (header->magic != MAGIC) {
		BSOD::die(Error::InvalidELFImage, "Keine gueltige ELF-Magic!\n");
		return;
	}

	ph = (ProgramHeader*)(((char*) header) + header->ph_offset);
	for (i = 0; i < header->ph_entry_count; i++, ph++) {
		void* dest = (void*) ph->virt_addr;
		void* src = ((char*) header) + ph->offset;

		// Nur Program Header vom Typ LOAD laden
		if (ph->type != 1) {
			continue;
		}
		
		if(ph->virt_addr < entryPointAddress) {
			BSOD::die(Error::InvalidELFImage, "A LOAD section tried to sneak into the kernel!");
		}

		for(uint32_t i = 0; i < ph->mem_size; i += 0x1000) {
			kernelContext->provide(
				virtual_t(ph->virt_addr + i),
				VMMFlags::Writable | VMMFlags::UserSpace);
		}
		
		memset(dest, 0, ph->mem_size);
		memcpy(dest, src, ph->file_size);
	}
	
	using EntryPoint = void (*)();
	EntryPoint ep = (EntryPoint)entryPointAddress;
	ep();
}

static void dump_elf(elf::Header *header)
{
	using namespace elf;
	ProgramHeader *ph;
	int i;

	// Ist es ueberhaupt eine ELF-Datei? 
	if (header->magic != MAGIC) {
		BSOD::die(Error::InvalidELFImage, "Keine gueltige ELF-Magic!\n");
		return;
	}
	ph = (ProgramHeader*)(((char*) header) + header->ph_offset);
	for (i = 0; i < header->ph_entry_count; i++, ph++) {
		void* dest = (void*) ph->virt_addr;
		void* src = ((char*) header) + ph->offset;

		Console::main
			<< "Header: "   << ph->type << ", "
			<< "Source: "   << src  << ", "
			<< "Dest: "     << dest << ", "
			<< "Memsize: "  << ph->mem_size << ", "
			<< "Filesize: " << ph->file_size 
			<< "\n";
	}
}
*/

static void strcpy(char *dst, const char *src)
{
	while((*dst++ = *src++));
}

static void initializePMM(Structure const & data)
{
	for(auto &mmap : data.memoryMaps) {
		if(mmap.length == 0) {
			continue;
    }
    if(mmap.isFree() == false) {
      continue;
    }
		
    //Console::main
		//<< "mmap: "
		//<< "start: " << hex(mmap.base) << ", length: " << hex(mmap.length)
		//<< ", " << mmap.entry_size
		//<< ", " << sizeof(mmap)
		//<< "\n";
    if(mmap.base > 0xFFFFFFFF) {
        //Console::main << "mmap out of 4 gigabyte range." << "\n";
        continue;
    }    
    if(mmap.isFree()) {
      // Mark all free memory free...
      physical_t lower = physical_t(mmap.base).alignUpper(4096);
      physical_t upper = physical_t(mmap.base + mmap.length).alignLower(4096);
      
      uint32_t ptr = lower.numeric();
      while (ptr < upper.numeric()) {
        PMM::markFree(physical_t(ptr));
        ptr += 0x1000;
      }
    }
  }
  
  // Mark all memory used by the kernel used...
  physical_t lower = physical_t(&kernelStartMarker).alignLower(4096);
  physical_t upper = physical_t(&kernelEndMarker).alignUpper(4096);
  
  uint32_t ptr = lower.numeric();
  while (ptr < upper.numeric()) {
    PMM::markUsed(physical_t(ptr));
    ptr += 0x1000;
  }
	
	// nullptr is not valid.
	PMM::markUsed(physical_t(nullptr));
	
	// Mark the video memory as used.
	PMM::markUsed(physical_t(0xB8000));
}

extern "C" void init(Structure const & data)
{
	Console::main
		<< "Hello World!\n"
		<< FColor(Color::Yellow) << "Hello color!" << FColor() << "\n"
		<< BColor(Color::Blue) << "Hello blue!" << BColor() << "\n"
		<< "Hello default color.\n";
  
	GDT::initialize();
	
  Console::main
    << "bootloader name:  " << data.bootLoaderName << "\n"
    << "command line:     " << data.commandline << "\n"
    << "count of modules: " << data.modules.length << "\n"
    << "count of mmaps:   " << data.memoryMaps.length << "\n";
  
	initializePMM(data);
  
  auto freeMemory = PMM::getFreeMemory();
  Console::main
    << "Free: "
    << (freeMemory >> 20) << "MB, "
    << (freeMemory >> 10) << "KB, "
    << (freeMemory >>  0) << "B, "
    << (freeMemory >> 12) << "Pages\n";

	IDT::initialize();
	Console::main << "Interrupts set up.\n";
	
	Console::main << "Creating VMM Context...\n";
	kernelContext = new (PMM::alloc().data()) VMMContext();
	
	Console::main << "Mapping memory...\n";
	for(uint32_t addr = 0; addr < 4096 * 1024; addr += 0x1000) {
		kernelContext->map(
			virtual_t(addr), 
			physical_t(addr),
			VMMFlags::Writable | VMMFlags::UserSpace);
	}
	kernelContext->map(
		virtual_t(kernelContext),
		physical_t(kernelContext),
		VMMFlags::Writable);
	Console::main << "Active Context...\n";
	VMM::activate(*kernelContext);
	
	Console::main << "Active Paging...\n";
	VMM::enable();
	Console::main << "Virtual Memory Management ready.\n";
	
	timer.install();
	keyboardDriver.install();
	
	Console::main << "Drivers installed.\n";
	
	//asm volatile("sti");
	ASM::sti();
	
	Console::main << "Interrupts enabled.\n";
	
	if(data.modules.length > 0)
	{
		// Console::main << "ELF Modukle:\n";
		// dump_elf(data.modules[0].start.data<elf::Header>());
		// run_program0(data.modules[0]);
		
		vm::Module module = {
			.code = reinterpret_cast<vm::Instruction*>(data.modules[0].start.data()),
			.length = data.modules[0].size() / sizeof(vm::Instruction),
		};
		
		
		Console::main << "Loaded instructions: " << module.length << "\n";
		
		uint8_t page0[64];
		uint8_t page1[64];
		
		strcpy((char*)page0, "Hallo Welt!\nDies ist die erste Ausgabe durch die VM.");
		
		uint8_t *pages[2];
		pages[0] = page0;
		pages[1] = page1;
		
		
		vm::VirtualMemoryMap mmap = {
			.pageSize = 64,
			.length = 2,
			.pages = pages,
		};
		
		vm::Process process = { 
			.module = &module, 
			.tag = (void*)1, 
			.codePointer = 0,
			.stackPointer = 0, 
			.basePointer = 0, 
			.flags = 0, 
			.stack = { 0 },
			.mmap = mmap,
		};
		
		auto *p = &process;
		
		while(vm::vm_step_process(p) && p->tag) {
			//Console::main << "?";
			// dump_proc(p);
		}
		
	}
	
  while(true);
}

static_assert(sizeof(void*) == 4, "Target platform is not 32 bit.");




namespace vm 
{
	extern "C" void vm_assert(int assertion, const char *msg)
	{
		if(assertion != 0)
			return;
		BSOD::die(Error::VMError, msg);
	}

	extern "C" void vm_syscall(Process *p, CommandInfo *info)
	{
		switch(info->additional)
		{
			case 0: // EXIT
				p->tag = NULL;
				break;
			case 1:
				Console::main << (char)info->input0;
				break;
			case 2:
				Console::main << info->input0;
				break;
			default:
				Console::main << "[SYSCALL " << info->additional << "]";
				break;
			}
	}

	extern "C" void vm_hwio(Process *p, CommandInfo *info)
	{
		BSOD::die(Error::VMError, "hwio not implemented yet.");
	}
}