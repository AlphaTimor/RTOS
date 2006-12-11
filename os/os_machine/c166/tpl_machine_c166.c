#pragma SRC
/*
 * Trampoline OS
 *
 * Trampoline is copyright (c) IRCCyN 2005+
 * Trampoline est prot��par la loi sur la propri��intellectuelle
 *
 * This software is distributed under the Lesser GNU Public Licence
 *
 * Trampoline C166 specifics
 *
 * $Date:$
 * $Rev$
 * $Author$
 * $URL$
 */

#include "tpl_machine.h"
#include "tpl_os_application_def.h"
#include "tpl_os_generated_configuration.h"	   /* TASK_COUNT and ISR_COUNT*/
#include "tpl_os_definitions.h" /* IS_ROUTINE */
#include <C167CS.H>	/*TODO: update with a more generic standard include file.*/
#include <stdio.h>


#pragma warning disable = 47 /* disables the "unreferenced parameter" warning */
							 /* used for the tpl_switch_context function      */


c166_context idle_task_context;


#define SMALL_MEMORY_MODEL

#ifdef SMALL_MEMORY_MODEL
/* use a register bank for interrupts and in the context switch function */
unsigned int idata registers_it[16];
unsigned int idata registers_c166[TASK_COUNT+ISR_COUNT+1][16];

/*
 * tpl_sleep is used by the idle task
 */
void tpl_sleep(void)
{
    while (1);
}

void tpl_shutdown(void)
{
	/* remove ITs */
	IEN = 0;
	/* TODO: update to set idle mode.*/
	while (1); 
}

void tpl_switch_context(tpl_context *old_context, tpl_context *new_context)
{
	
	/* Only works with a near memory model.
	 *  
	 * R8 has a pointer to the old context.
	 * R9 has a pointer to the new context.
	 * first of all: store the current context in old_context
	 * R1 stores DPP3 of the context we're working on 
	 * R5 stores the context we're working on
	 * R2 stores intermediate values (IP, stack)
	 * R3 stores the old CP 
	 */
	#pragma asm
	PUSH CP /*store the current register bank */
	MOV CP, #registers_it
	NOP /* pipeline effect */
	POP R3 /* the old CP */
	/* store DPP3 on R1 */
	MOV R1, DPP3
	MOV DPP3, #3
	NOP
	MOV R0, [R3] //R0 in user stack.
	MOV R8, [R3+#16]
	MOV R9, [R3+#18]
	#ifdef LARGE_POINTER 
		/* pointer does not fill in only one 16 bit register*/
		MOV R10,[R3+#20] 
		MOV R11,[R3+#22]
	#endif
	/*ok, now, all the other registers may be changed. */
	/* if old_context */
	#pragma endasm
	if(old_context) /*don't store context if pointer is NULL. */
	{
		#pragma asm
			/* R5 get the context */
			MOV R5,[R8]
		
			MOV [R5],MDC	/* store mdc  */
			ADD R5,#2	
			MOV [R5],DPP0	/* store dpp0 */
			ADD R5,#2
			MOV [R5],DPP1	/* store dpp1 */
			ADD R5,#2
			MOV [R5],DPP2	/* store dpp2 */
			ADD R5,#2
			MOV [R5],R1	/* store dpp3 */
			ADD R5,#2
			MOV [R5],MDH	/* store mdh */
			ADD R5,#2
			MOV [R5],MDL	/* store mdl */
			POP R2		/*R2 get return ip*/
			ADD R5,#2
			MOV [R5],SP	/* store sp */
			ADD R5,#2
			MOV [R5],STKOV	/* store stkov */
			ADD R5,#2
			MOV [R5],STKUN	/* store stkun */
			ADD R5,#2
			MOV [R5],PSW	/* store psw */
			ADD R5,#2
			MOV [R5],CSP	/* store csp */
			ADD R5,#2
			MOV [R5],R2	/* store ip */
			ADD R5,#2
			MOV [R5],R3	/* store cp */
			ADD R5,#2
			MOV [R5],R0	/* store user stack */
		#pragma endasm
	}
	/* then set the new context */
	#pragma asm
		MOV R5, [R9] /*the new context */
		MOV MDC, [R5]	/* restore mdc */
		ADD R5,#2
		MOV DPP0, [R5]	/* restore dpp0 */
		ADD R5,#2           /* no dpp use allowed after writing in DPPx, pipeline effect */
		MOV DPP1, [R5]	/* restore dpp1 */
		ADD R5,#2
		MOV DPP2, [R5]	/* restore dpp2 */
		ADD R5,#2
		MOV R1, [R5]	/* get the dpp3 to restore at the end.*/
		ADD R5,#2
		MOV MDH, [R5]	/* restore mdh */
		ADD R5,#2
		MOV MDL, [R5]	/* restore mdl */
		/* no exception after a stkov or stkun change.
		 * only after a sp change -> not true with the monitor.
 		 */

		MOV R2,#0xf200   /*low limit of iram -> stkov. */
        	MOV STKOV,R2	
	        MOV R2,#0xfdfe   /*high limit of iram -> stkun */
	        MOV STKUN,R2

		/* then set the new sp */
		ADD R5,#2
		MOV SP, [R5]	/* restore sp ********* STACK CHANGE **********/
		ADD R5,#2
		MOV STKOV, [R5]	/* restore stkov */
		ADD R5,#2
		MOV STKUN, [R5]	/* restore stkun */

		/* store PSW, CSP and IP on top of the stack -> RETI*/
		ADD R5,#2
		MOV R2, [R5]	/* get PSW */
		PUSH R2		/* store in stack */
		ADD R5,#2
		MOV R2, [R5]	/* get CSP */
		PUSH R2		/* store in stack */
		ADD R5,#2
		MOV R2, [R5]	/* get IP */
		PUSH R2		/* store in stack */

		ADD R5,#2
		MOV R2, [R5] //R2 <- CP of the new task
		ADD R5,#2
		MOV [R2], [R5] //init R0 of the new task.
		PUSH R1 		/*store DPP3 on stack */
		MOV CP, R2	/* restore cp */
		/* Warning next instruction MUST NOT use GPRs!!! (pipeline effect) */
		POP DPP3		/* restore DPP3 */
		/* the next "reti" instruction will pop the IP, CSP and PSW registers */
		RETI
	#pragma endasm
}

void tpl_switch_context_from_it(tpl_context * old_context, tpl_context * new_context)
{
	/* Only works with a near memory model.
	 * current register bank is registers_it
	 * R8 has a pointer to the old context.
	 * R9 has a pointer to the new context.
	 * DPP3 = 3 (value set in interrupt handler).
	 * first of all: store the current context in old_context
	 */
	if(old_context) /*don't store context if pointer is NULL. */
	{
		/* R1, R2 used to get values from the stack (see documentation). */
		/* R5 used to get fields in the context. */
		#pragma asm
			MOV R5,[R8]		/* init*/
			MOV R1,0xfe12   

			MOV [R5], MDC	/* store mdc  */
			
			MOV R2, [R1+#14]
			ADD R5,#2
			MOV [R5],R2		/* store dpp0 */
			
			ADD R5,#2
			MOV [R5],DPP1	/* store dpp1 */
			ADD R5,#2
			MOV [R5],DPP2	/* store dpp2 */

			MOV R2, [R1+#18]
			ADD R5,#2
			MOV [R5],R2		/* store dpp3 */
			
			ADD R5,#2
			MOV [R5],MDH		/* store mdh */

			ADD R5,#2
			MOV [R5],MDL		/* store mdl */
			
			MOV R2, R1 		 
			ADD R2, #26
			ADD R5,#2
			MOV [R5],R2		 /* store sp */
			
			ADD R5,#2
			MOV [R5],STKOV	/* store stkov */
			ADD R5,#2
			MOV [R5],STKUN	/* store stkun */

			MOV R2, [R1+#24]
			ADD R5,#2
			MOV [R5],R2		/* store psw */

			MOV R2, [R1+#22]
			ADD R5,#2
			MOV [R5],R2		/* store csp */

			MOV R2, [R1+#20]
			ADD R5,#2
			MOV [R5],R2		/* store ip */

			MOV R2, [R1+#16]
			ADD R5,#2
			MOV [R5],R2	/* store cp */
		
			ADD R5,#2
			MOV [R5],R0	/* store user stack */
			
		#pragma endasm
	}
	/* then set the new context */
	#pragma asm
		MOV R5, [R9] /*the new context */
		MOV MDC, [R5]	/* restore mdc */
		ADD R5,#2
		MOV DPP0, [R5]	/* restore dpp0 */
		ADD R5,#2           /* no dpp use allowed after writing in DPPx, pipeline effect */
		MOV DPP1, [R5]	/* restore dpp1 */
		ADD R5,#2
		MOV DPP2, [R5]	/* restore dpp2 */
		ADD R5,#2
		MOV R1, [R5]	/* get the dpp3 to restore at the end.*/
		ADD R5,#2
		MOV MDH, [R5]	/* restore mdh */
		ADD R5,#2
		MOV MDL, [R5]	/* restore mdl */
		/* no exception after a stkov or stkun change.
		 * only after a sp change -> not true with the monitor.
 		 */

		MOV R2,#0xf200   /*low limit of iram -> stkov. */
		MOV STKOV,R2	
		MOV R2,#0xfdfe   /*high limit of iram -> stkun */
		MOV STKUN,R2

		/* then set the new sp */
		ADD R5,#2
		MOV SP, [R5]	/* restore sp ********* STACK CHANGE **********/
		ADD R5,#2
		MOV STKOV, [R5]	/* restore stkov */
		ADD R5,#2
		MOV STKUN, [R5]	/* restore stkun */

		/* store PSW, CSP and IP on top of the stack -> RETI*/
		ADD R5,#2
		MOV R2, [R5]	/* get PSW */
		PUSH R2		/* store in stack */
		ADD R5,#2
		MOV R2, [R5]	/* get CSP */
		PUSH R2		/* store in stack */
		ADD R5,#2
		MOV R2, [R5]	/* get IP */
		PUSH R2		/* store in stack */

		ADD R5,#2
		MOV R2, [R5] //R2 <- CP of the new task
		ADD R5,#2
		MOV [R2], [R5] //init R0 of the new task.
		PUSH R1 		/*store DPP3 on stack */
		MOV CP, R2	/* restore cp */
		/* Warning next instruction MUST NOT use GPRs!!! (pipeline effect) */
		POP DPP3		/* restore DPP3 */
		/* the next "reti" instruction will pop the IP, CSP and PSW registers */
		RETI
	#pragma endasm
}
#endif

/*
 * tpl_init_context initialize a context to prepare a task to run.
 */
void tpl_init_context(tpl_task *task)
{
	int i;
	int objId;

	/* Gets a pointer to the static descriptor of the task whose context is going to be inited */
	tpl_exec_static * static_desc = task->exec_desc.static_desc;

	/* Gets a pointer to the context going to be initialized */
	c166_context *ic = static_desc->context.ic;

	/* find id of the tpl_exec_obj  */
	objId = static_desc->id;
    if (static_desc->type & IS_ROUTINE) objId += TASK_COUNT;

	/* CP init */
	ic->cp = (unsigned int)&(registers_c166[objId + 1][0]);

	/*GPRs sets to 0 : for debug mode*/
	for(i = 0; i < 16; i++) registers_c166[objId + 1][i] = 0;

	/* Init of the system stack and storage of the task's entry point address on system stack */
	ic->stkun = ((unsigned int)(static_desc->stack.sys_stack_zone)) + 
				static_desc->stack.sys_stack_size;
	ic->stkov = (unsigned int)(static_desc->stack.sys_stack_zone);
	/* 6 bytes reserved to push the init value of IP/CSP/PSW on the stack 
	 * the context switch uses the RETI instruction to switch to the task function.
	 */
	ic->sp = 	ic->stkun - 3*sizeof(unsigned int);
	/* pushes entry point on stack */
	ic->psw = 0x0800;
	ic->csp = (unsigned int)(((unsigned long)(static_desc->entry)) >> 16); 
	ic->ip = (unsigned int)(static_desc->entry); 
	/* Init of the user stack :  R0 = upper address of the user stack zone */
	ic->ustack = (unsigned int)(static_desc->stack.user_stack_zone) + 
									static_desc->stack.user_stack_size;  
	/* Init Others to 0 */
	ic->mdh = 0;
	ic->mdl = 0;
    /* Init DPPx registers */
#ifdef SMALL_MEMORY_MODEL
	ic->dpp0 = DPP0;
#else
	ic->dpp0 = (unsigned int)((unsigned long)ic >> 14);
#endif
	ic->dpp1 = DPP1;
	ic->dpp2 = DPP2;
	ic->dpp3 = DPP3;
}



void tpl_get_task_lock(void)
{
	IEN = 0;
}

void tpl_release_task_lock(void)
{
	IEN = 1;
}

/*
 * tpl_init_machine 
 */
void tpl_init_machine(void)
{

}
