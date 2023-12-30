#include "breakpoint_mngr.hpp"
#include "debugger.hpp"
#include "tracee.hpp"


void BreakpointMngr::parseModuleBrkPnt(std::string &brk_mod_addr)
{
    std::list<Breakpoint *> brk_offset;
    m_log->trace("BRK {}", brk_mod_addr.c_str());

    auto mod_idx = brk_mod_addr.find("@");
    std::string mod_name = brk_mod_addr.substr(0, mod_idx);
    m_log->trace("Module {}", mod_name.c_str());

    int pnt_idx = mod_idx, prev_idx = mod_idx;

    while (pnt_idx > 0)
    {
        prev_idx = pnt_idx + 1;
        pnt_idx = brk_mod_addr.find(",", prev_idx);
        uint64_t mod_offset = 0;
        if (pnt_idx > 0)
            mod_offset = stoi(brk_mod_addr.substr(prev_idx, pnt_idx - prev_idx), 0, 16);
        else
            mod_offset = stoi(brk_mod_addr.substr(prev_idx), 0, 16);
        m_log->trace("  Off {:x}", mod_offset);

        brk_offset.push_back(new Breakpoint(mod_name, mod_offset));
    }

    m_pending[mod_name] = brk_offset;
}

void BreakpointMngr::addBrkPnt(Breakpoint *brkPtr)
{

    std::list<Breakpoint *> brk_offset;

    auto pnd_brk_iter = m_pending.find(brkPtr->m_modname);
    if (pnd_brk_iter != m_pending.end())
    {
        brk_offset = pnd_brk_iter->second;
        m_log->error("Module found!");
    }

    brk_offset.push_back(brkPtr);
    m_pending[brkPtr->m_modname] = brk_offset;
}

// put all the pending breakpoint in the tracee
void BreakpointMngr::inject(DebugOpts& debug_opts)
{
    debug_opts.m_procMap.print();
    m_log->trace("Yeeahh... Injecting all the pending Breakpoints!");
    
    BreakpointInjector* brkPntInjector;
    
    /*
    if (m_target_desc.m_cpu_arch == CPU_ARCH::AMD64 || m_target_desc.m_cpu_arch == CPU_ARCH::X86) {
        brkPntInjector = new X86BreakpointInjector();
    } else if(m_target_desc.m_cpu_arch == CPU_ARCH::ARM32) {
        brkPntInjector = new ARMBreakpointInjector();
    } else if(m_target_desc.m_cpu_arch == CPU_ARCH::ARM64) {
        brkPntInjector = new ARM64BreakpointInjector();
    }
    */

    for (auto pend_iter = m_pending.cbegin(); pend_iter != m_pending.cend();)
    {
        // find the module base address
        std::string mod_name = pend_iter->first;
        auto mod_base_addr = debug_opts.m_procMap.findModuleBaseAddr(mod_name);

        // iterate over all the breakpoint for that module
        // for(auto brkpnt_obj: pend_iter->second) {
        auto brk_pending_objs = pend_iter->second;
        while (!brk_pending_objs.empty())
        {
            Breakpoint *brkpnt_obj = brk_pending_objs.back();
            // brkpnt_obj->setInjector(brkPntInjector);
            uintptr_t brk_addr = mod_base_addr + brkpnt_obj->m_offset;
            m_log->debug("Setting Brk at addr : 0x{:x}", brk_addr);
            brkpnt_obj->setAddress(brk_addr);
            brkpnt_obj->enable(debug_opts);
            m_log->trace("This is debug stop!");
            // brkpnt_obj->addPid(debug_opts.getPid());
            m_active_brkpnt[brk_addr] = brkpnt_obj;
            // auto bb_obj = placeSingleStepBreakpoint(debug_opts, brk_addr + 4);
            // m_active_brkpnt[brk_addr + 4] = bb_obj.release(); 
            brk_pending_objs.pop_back();
        }
        pend_iter = m_pending.erase(pend_iter); // or "it = m.erase(it)" since C++11
    }
    m_log->trace("All breakpoints injected!");
}

Breakpoint* BreakpointMngr::getBreakpointObj(uintptr_t bk_addr)
{
    auto brk_pnt_iter = m_active_brkpnt.find(bk_addr);
    if (brk_pnt_iter != m_active_brkpnt.end())
    {
        // breakpoint is found, its under over management
        auto brk_obj = brk_pnt_iter->second;
        return brk_obj;
    }
    else
    {
        m_log->error("No Breakpoint object found! This is very unusual!");
        return nullptr;
    }
}

void BreakpointMngr::restoreSuspendedBreakpoint(TraceeProgram& traceeProgram)
{
    DebugOpts& debug_opts = traceeProgram.getDebugOpts();

#if defined(SUPPORT_ARCH_ARM)
    std::unique_ptr<BranchData> branch_info_brkpt = std::move(traceeProgram.m_single_step_brkpnt);
    m_log->debug("Restoring breakpoint and resuming execution!");
    branch_info_brkpt->m_target_brkpt->disable(debug_opts);
    if(branch_info_brkpt->m_fall_target)
        branch_info_brkpt->m_fall_target_brkpt->disable(debug_opts);
    m_branch_info_cache[branch_info_brkpt->addr()] = std::move(branch_info_brkpt);
#endif

    auto sus_bkpt_iter = m_suspendedBrkPnt.find(debug_opts.m_pid);
    if (sus_bkpt_iter != m_suspendedBrkPnt.end()) {
        // tracee is found, its under over management
        auto suspend_bkpt_obj = sus_bkpt_iter->second;

        if (suspend_bkpt_obj->shouldEnable()) {
            suspend_bkpt_obj->enable(debug_opts);
            m_log->trace("Restoring");
        } else {
            m_log->trace("Not restoring");
            // although we don't need breakpoint object we are not deleting 
            // it that because it will be later used to summarize 
            // execution information
        }
        m_suspendedBrkPnt.erase(debug_opts.m_pid);
    } else {
        m_log->info("No suspended breakpoint found!");
        auto suspend_bkpt_obj = nullptr;
    }
}

void BreakpointMngr::handleBreakpointHit(DebugOpts& debug_opts, uintptr_t brk_addr)
{
    // PC points to the next instruction after execution
    m_log->trace("Breakpoint Hit! addr 0x{:x}", brk_addr);
    // find the breakpoint object for further processing
    auto brk_obj = getBreakpointObj(brk_addr);
    if (brk_obj == nullptr) {
        m_log->trace("No Breakpoint Handler found!");
        exit(-1);
        return;
    }

    m_suspendedBrkPnt[debug_opts.m_pid] = brk_obj;
    brk_obj->handle(debug_opts);
    
    // m_log->debug("Brkpnt obj found!");
    // restore the value of original breakpoint instruction
    brk_obj->disable(debug_opts);
    // return brk_obj;
}

void BreakpointMngr::printStats()
{
    uint64_t bkpt_count = 0, bkpt_total = 0, brk_pt_exec_cnt = 0;
    m_log->info("------[ Breakpoint Stats ]-----");
    for (auto i = m_active_brkpnt.begin(); i != m_active_brkpnt.end(); i++)
    {
        auto brk_pt = i->second;
        bkpt_total +=1;
        if (brk_pt->getHitCount() > 0) {
            bkpt_count += 1;
            brk_pt_exec_cnt += brk_pt->getHitCount();
        }
        // m_log->info("{} {}", brk_pt->m_label.c_str(), brk_pt->getHitCount());
    }
    m_log->info("Number Of Breakpoint Hits : {}/{}", bkpt_count, bkpt_total);
    m_log->info("Total Breakpoint Hits     : {}", brk_pt_exec_cnt);
    m_log->info("[------------------------------");
};

void BreakpointMngr::placeSingleStepBreakpoint(uintptr_t brkpt_hit_addr, TraceeProgram& traceeProgram) {

    DebugOpts& debug_opts = traceeProgram.getDebugOpts();
    auto branch_info_iter = m_branch_info_cache.find(brkpt_hit_addr);
    
    if (branch_info_iter != m_branch_info_cache.end()) {
        // tracee is found, its under over management
        std::unique_ptr<BranchData> ss_bkpt_obj = std::move(branch_info_iter->second);
        ss_bkpt_obj->m_target_brkpt->enable(debug_opts);
        if(ss_bkpt_obj->m_fall_target) {
            ss_bkpt_obj->m_fall_target_brkpt->enable(debug_opts);
        }
        traceeProgram.m_single_step_brkpnt = std::move(ss_bkpt_obj);
    } else {
        m_log->info("No Branch data found!");
        std::unique_ptr<BranchData> branch_info(new BranchData(brkpt_hit_addr));
        // we need this instruction data in case we are in computed target instruction
        // and we need to emulate the instruction everytime.
        Addr* inst_data = debug_opts.m_memory.readPointerObj(brkpt_hit_addr, 4);
        m_arm_disasm->getBranchInfo(inst_data->data(), *branch_info, debug_opts);
        // branch_info->print();

        m_log->debug("Target breakpoint at 0x{:x}", branch_info->m_target);
        std::unique_ptr<Breakpoint> targetBranchBkpt(new Breakpoint(*new std::string("single-stop-target"), 0));
        // targetBranchBkpt->setInjector(new ARMBreakpointInjector());
        targetBranchBkpt->makeSingleStep(branch_info->m_target);
        targetBranchBkpt->enable(debug_opts);
        branch_info->m_target_brkpt = std::move(targetBranchBkpt);
        if(branch_info->m_fall_target) {
            m_log->debug("Fall through breakpoint at 0x{:x}", branch_info->m_fall_target);
            std::unique_ptr<Breakpoint> targetFallBranchBkpt(new Breakpoint(*new std::string("single-stop-fall-target"), 0));
            targetFallBranchBkpt->makeSingleStep(branch_info->m_fall_target);
            targetFallBranchBkpt->enable(debug_opts);
            branch_info->m_fall_target_brkpt = std::move(targetFallBranchBkpt);
        }
        traceeProgram.m_single_step_brkpnt = std::move(branch_info);
    }
    // TODO : not sure if this object should be recorded somewhere?
    // currently it stored and restored by Debugger class
    // m_active_brkpnt[brk_addr] = targetBranchBkpt;
    // return targetBranchBkpt;
};