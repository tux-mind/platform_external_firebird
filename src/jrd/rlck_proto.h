/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		rlck_proto.h
 *	DESCRIPTION:	Prototype header file for rlck.cpp
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#ifndef JRD_RLCK_PROTO_H
#define JRD_RLCK_PROTO_H

#ifdef PC_ENGINE

class Lock;
class jrd_rel;
class jrd_tra;
struct record_param;
struct blk;
class Attachment;

Lock* RLCK_lock_record(record_param*, USHORT, lock_ast_t, blk*);
									
Lock* RLCK_lock_record_implicit(jrd_tra*, record_param*,
											 USHORT, lock_ast_t, blk*);
Lock* RLCK_lock_relation(jrd_rel*, USHORT, lock_ast_t, blk*);
Lock* RLCK_range_relation(jrd_tra*, jrd_rel*, lock_ast_t, blk*);
Lock* RLCK_record_locking(jrd_rel*);
void RLCK_release_lock(Lock*);
void RLCK_release_locks(class Attachment*);
#endif
Lock* RLCK_reserve_relation(struct thread_db*, jrd_tra*,
										 jrd_rel*, bool, bool);

/* TMN: This header did not match the implementation.
 * I moved the #ifdef as noted
 */
/* #ifdef PC_ENGINE */
void RLCK_shutdown_attachment(class Attachment*);
void RLCK_shutdown_database(class Database*);
#ifdef PC_ENGINE
void RLCK_signal_refresh(jrd_tra*);
#endif

Lock* RLCK_transaction_relation_lock(jrd_tra*, jrd_rel*);

#ifdef PC_ENGINE
void RLCK_unlock_record(Lock*, record_param*);
void RLCK_unlock_record_implicit(Lock*, record_param*);
void RLCK_unlock_relation(Lock*, jrd_rel*);
#endif

#endif // JRD_RLCK_PROTO_H

