/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Vlad Khorsun
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2008 Vlad Khorsun <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "fb_types.h"
#include "../common.h"
#include "../../include/fb_blk.h"

#include "../align.h"
#include "../exe.h"
#include "../jrd.h"
#include "../tra.h"
#include "../dsc.h"
#include "../../dsql/dsql.h"
#include "../../dsql/sqlda_pub.h"

#include "../blb_proto.h"
#include "../evl_proto.h"
#include "../exe_proto.h"
#include "../mov_proto.h"
#include "../mov_proto.h"
#include "../PreparedStatement.h"
#include "../Function.h"

#include "InternalDS.h"

using namespace Jrd;
using namespace Firebird;

namespace EDS {

const char* INTERNAL_PROVIDER_NAME = "Internal";

class RegisterInternalProvider
{
public:
	RegisterInternalProvider()
	{
		InternalProvider* provider = new InternalProvider(INTERNAL_PROVIDER_NAME);
		Manager::addProvider(provider);
	}
};

static RegisterInternalProvider reg;

// InternalProvider

void InternalProvider::jrdAttachmentEnd(thread_db* tdbb, Jrd::Attachment* att)
{
	if (m_connections.getCount() == 0)
		return;

	Connection** ptr = m_connections.end();
	Connection** begin = m_connections.begin();

	for (ptr--; ptr >= begin; ptr--)
	{
		InternalConnection* conn = (InternalConnection*) *ptr;
		if (conn->getJrdAtt() == att)
			releaseConnection(tdbb, *conn, false);
	}
}

void InternalProvider::getRemoteError(ISC_STATUS* status, string& err) const
{
	err = "";

	char buff[1024];
	const ISC_STATUS* p = status;
	const ISC_STATUS* end = status + ISC_STATUS_LENGTH;

	while (p < end)
	{
		const ISC_STATUS code = *p ? p[1] : 0;
		if (!fb_interpret(buff, sizeof(buff), &p))
			break;

		string rem_err;
		rem_err.printf("%lu : %s\n", code, buff);
		err += rem_err;
	}
}

Connection* InternalProvider::doCreateConnection()
{
	return new InternalConnection(*this);
}


// InternalConnection

InternalConnection::~InternalConnection()
{
}

void InternalConnection::attach(thread_db* tdbb, const Firebird::string& dbName,
		const Firebird::string& user, const Firebird::string& pwd,
		const Firebird::string& role)
{
	fb_assert(!m_attachment);
	Database* dbb = tdbb->getDatabase();
	fb_assert(dbName.isEmpty() || dbName == dbb->dbb_database_name.c_str());

	// Don't wrap raised errors. This is needed for backward compatibility.
	setWrapErrors(false);

	Jrd::Attachment* attachment = tdbb->getAttachment();
	if ((user.isEmpty() || user == attachment->att_user->usr_user_name) &&
		pwd.isEmpty() &&
		(role.isEmpty() || role == attachment->att_user->usr_sql_role_name))
	{
		m_isCurrent = true;
		m_attachment = attachment;
	}
	else
	{
		m_isCurrent = false;
		m_dbName = dbb->dbb_database_name.c_str();
		generateDPB(tdbb, m_dpb, user, pwd, role);

		ISC_STATUS_ARRAY status = {0};

		{
			EngineCallbackGuard guard(tdbb, *this);
			jrd8_attach_database(status, 0, m_dbName.c_str(), &m_attachment,
				m_dpb.getBufferLength(), m_dpb.getBuffer());
		}
		if (status[1]) {
			raise(status, tdbb, "attach");
		}
	}

	m_sqlDialect = (m_attachment->att_database->dbb_flags & DBB_DB_SQL_dialect_3) ?
					SQL_DIALECT_V6 : SQL_DIALECT_V5;
}

void InternalConnection::doDetach(thread_db* tdbb)
{
	fb_assert(m_attachment);

	if (m_isCurrent)
	{
		m_attachment = 0;
	}
	else
	{
		ISC_STATUS_ARRAY status = {0};

		{	// scope
			Jrd::Attachment* att = m_attachment;
			m_attachment = NULL;

			EngineCallbackGuard guard(tdbb, *this);
			jrd8_detach_database(status, &att);

			m_attachment = att;
		}

		if (status[1] == isc_att_shutdown)
		{
			m_attachment = NULL;
			fb_utils::init_status(status);
		}
		if (status[1])
		{
			raise(status, tdbb, "detach");
		}
	}

	fb_assert(!m_attachment)
}

bool InternalConnection::cancelExecution(thread_db* tdbb)
{
	if (m_isCurrent)
		return true;

	ISC_STATUS_ARRAY status = {0, 0, 0};
	jrd8_cancel_operation(status, &m_attachment, fb_cancel_raise);
	return (status[1] == 0);
}

// this internal connection instance is available for the current execution context if it
// a) is current conenction and current thread's attachment is equal to
//	  this attachment, or
// b) is not current conenction
bool InternalConnection::isAvailable(thread_db* tdbb, TraScope /*traScope*/) const
{
	return !m_isCurrent ||
		(m_isCurrent && (tdbb->getAttachment() == m_attachment));
}

bool InternalConnection::isSameDatabase(thread_db* tdbb, const Firebird::string& dbName,
		const Firebird::string& user, const Firebird::string& pwd,
		const Firebird::string& role) const
{
	if (m_isCurrent)
	{
		const UserId* attUser = m_attachment->att_user;
		return ((user.isEmpty() || user == attUser->usr_user_name) &&
				pwd.isEmpty() &&
				(role.isEmpty() || role == attUser->usr_sql_role_name));
	}

	return Connection::isSameDatabase(tdbb, dbName, user, pwd, role);
}

Transaction* InternalConnection::doCreateTransaction()
{
	return new InternalTransaction(*this);
}

Statement* InternalConnection::doCreateStatement()
{
	return new InternalStatement(*this);
}

Blob* InternalConnection::createBlob()
{
	return new InternalBlob(*this);
}


// InternalTransaction()

void InternalTransaction::doStart(ISC_STATUS* status, thread_db* tdbb, ClumpletWriter& tpb)
{
	fb_assert(!m_transaction);

	if (m_scope == traCommon && m_IntConnection.isCurrent()) {
		m_transaction = tdbb->getTransaction();
	}
	else
	{
		Jrd::Attachment* att = m_IntConnection.getJrdAtt();

		EngineCallbackGuard guard(tdbb, *this);
		jrd8_start_transaction(status, 0, &m_transaction, 1, &att,	//// FIXME: public_handle
			tpb.getBufferLength(), tpb.getBuffer());
	}
}

void InternalTransaction::doPrepare(ISC_STATUS* /*status*/, thread_db* /*tdbb*/,
		int /*info_len*/, const char* /*info*/)
{
	fb_assert(m_transaction);
	fb_assert(false);
}

void InternalTransaction::doCommit(ISC_STATUS* status, thread_db* tdbb, bool retain)
{
	fb_assert(m_transaction);

	if (m_scope == traCommon && m_IntConnection.isCurrent()) {
		if (!retain) {
			m_transaction = NULL;
		}
	}
	else
	{
		EngineCallbackGuard guard(tdbb, *this);
		if (retain)
			jrd8_commit_retaining(status, &m_transaction);
		else
			jrd8_commit_transaction(status, &m_transaction);
	}
}

void InternalTransaction::doRollback(ISC_STATUS* status, thread_db* tdbb, bool retain)
{
	fb_assert(m_transaction);

	if (m_scope == traCommon && m_IntConnection.isCurrent()) {
		if (!retain) {
			m_transaction = NULL;
		}
	}
	else
	{
		EngineCallbackGuard guard(tdbb, *this);
		if (retain)
			jrd8_rollback_retaining(status, &m_transaction);
		else
			jrd8_rollback_transaction(status, &m_transaction);
	}

	if (status[1] == isc_att_shutdown && !retain)
	{
		m_transaction = NULL;
		fb_utils::init_status(status);
	}
}


// InternalStatement

InternalStatement::InternalStatement(InternalConnection& conn) :
	Statement(conn),
	m_intConnection(conn),
	m_intTransaction(0),
	m_request(0),
	m_inBlr(getPool()),
	m_outBlr(getPool())
{
}

InternalStatement::~InternalStatement()
{
}

void InternalStatement::doPrepare(thread_db* tdbb, const string& sql)
{
	m_inBlr.clear();
	m_outBlr.clear();

	Jrd::Attachment* att = m_intConnection.getJrdAtt();
	jrd_tra* tran = getIntTransaction()->getJrdTran();

	ISC_STATUS_ARRAY status = {0};
	if (!m_request)
	{
		fb_assert(!m_allocated);
		EngineCallbackGuard guard(tdbb, *this);
		jrd8_allocate_statement(status, &att, &m_request);
		m_allocated = (m_request != 0);
	}
	if (status[1]) {
		raise(status, tdbb, "jrd8_allocate_statement", &sql);
	}

	{
		EngineCallbackGuard guard(tdbb, *this);

		CallerName save_caller_name(tran->tra_caller_name);

		if (m_callerPrivileges)
		{
			jrd_req* request = tdbb->getRequest();
			JrdStatement* statement = request ? request->getStatement() : NULL;
			CallerName callerName;
			const Routine* routine;

			if (request && statement->triggerName.hasData())
				tran->tra_caller_name = CallerName(obj_trigger, statement->triggerName);
			else if (request && (routine = statement->getRoutine()) &&
				routine->getName().identifier.hasData())
			{
				if (routine->getName().package.isEmpty())
				{
					tran->tra_caller_name = CallerName(routine->getObjectType(),
						routine->getName().identifier);
				}
				else
				{
					tran->tra_caller_name = CallerName(obj_package_header,
						routine->getName().package);
				}
			}
			else
				tran->tra_caller_name = CallerName();
		}

		jrd8_prepare(status, &tran, &m_request, sql.length(), sql.c_str(),
			m_connection.getSqlDialect(), 0, NULL, 0, NULL);

		tran->tra_caller_name = save_caller_name;
	}
	if (status[1]) {
		raise(status, tdbb, "jrd8_prepare", &sql);
	}

	const DsqlCompiledStatement* statement = m_request->getStatement();

	if (statement->getSendMsg()) {
		try {
			PreparedStatement::parseDsqlMessage(statement->getSendMsg(), m_inDescs,
				m_inBlr, m_in_buffer);
			m_inputs = m_inDescs.getCount() / 2;
		}
		catch (const Exception&) {
			raise(tdbb->tdbb_status_vector, tdbb, "parse input message", &sql);
		}
	}
	else {
		m_inputs = 0;
	}

	if (statement->getReceiveMsg()) {
		try {
			PreparedStatement::parseDsqlMessage(statement->getReceiveMsg(), m_outDescs,
				m_outBlr, m_out_buffer);
			m_outputs = m_outDescs.getCount() / 2;
		}
		catch (const Exception&) {
			raise(tdbb->tdbb_status_vector, tdbb, "parse output message", &sql);
		}
	}
	else {
		m_outputs = 0;
	}

	m_stmt_selectable = false;

	switch (statement->getType())
	{
	case DsqlCompiledStatement::TYPE_SELECT:
	case DsqlCompiledStatement::TYPE_SELECT_UPD:
	case DsqlCompiledStatement::TYPE_SELECT_BLOCK:
		m_stmt_selectable = true;
		break;

	case DsqlCompiledStatement::TYPE_START_TRANS:
	case DsqlCompiledStatement::TYPE_COMMIT:
	case DsqlCompiledStatement::TYPE_ROLLBACK:
	case DsqlCompiledStatement::TYPE_COMMIT_RETAIN:
	case DsqlCompiledStatement::TYPE_ROLLBACK_RETAIN:
	case DsqlCompiledStatement::TYPE_CREATE_DB:
		ERR_build_status(status, Arg::Gds(isc_eds_expl_tran_ctrl));
		raise(status, tdbb, "jrd8_prepare", &sql);
		break;

	case DsqlCompiledStatement::TYPE_INSERT:
	case DsqlCompiledStatement::TYPE_DELETE:
	case DsqlCompiledStatement::TYPE_UPDATE:
	case DsqlCompiledStatement::TYPE_UPDATE_CURSOR:
	case DsqlCompiledStatement::TYPE_DELETE_CURSOR:
	case DsqlCompiledStatement::TYPE_DDL:
	case DsqlCompiledStatement::TYPE_GET_SEGMENT:
	case DsqlCompiledStatement::TYPE_PUT_SEGMENT:
	case DsqlCompiledStatement::TYPE_EXEC_PROCEDURE:
	case DsqlCompiledStatement::TYPE_SET_GENERATOR:
	case DsqlCompiledStatement::TYPE_SAVEPOINT:
	case DsqlCompiledStatement::TYPE_EXEC_BLOCK:
		break;
	}
}


void InternalStatement::doExecute(thread_db* tdbb)
{
	jrd_tra* transaction = getIntTransaction()->getJrdTran();

	ISC_STATUS_ARRAY status = {0};
	{
		EngineCallbackGuard guard(tdbb, *this);
		jrd8_execute(status, &transaction, &m_request,
			m_inBlr.getCount(), reinterpret_cast<const SCHAR*>(m_inBlr.begin()),
			0, m_in_buffer.getCount(), reinterpret_cast<const SCHAR*>(m_in_buffer.begin()),
			m_outBlr.getCount(), (SCHAR*) m_outBlr.begin(),
			0, m_out_buffer.getCount(), (SCHAR*) m_out_buffer.begin());
	}

	if (status[1]) {
		raise(status, tdbb, "jrd8_execute");
	}
}


void InternalStatement::doOpen(thread_db* tdbb)
{
	jrd_tra* transaction = getIntTransaction()->getJrdTran();

	ISC_STATUS_ARRAY status = {0};
	{
		EngineCallbackGuard guard(tdbb, *this);
		jrd8_execute(status, &transaction, &m_request,
			m_inBlr.getCount(), reinterpret_cast<const SCHAR*>(m_inBlr.begin()),
			0, m_in_buffer.getCount(), reinterpret_cast<const SCHAR*>(m_in_buffer.begin()),
			0, NULL, 0, 0, NULL);
	}
	if (status[1]) {
		raise(status, tdbb, "jrd8_execute");
	}
}


bool InternalStatement::doFetch(thread_db* tdbb)
{
	ISC_STATUS_ARRAY status = {0};
	ISC_STATUS res = 0;
	{
		EngineCallbackGuard guard(tdbb, *this);
		res = jrd8_fetch(status, &m_request,
			m_outBlr.getCount(), reinterpret_cast<const SCHAR*>(m_outBlr.begin()), 0,
			m_out_buffer.getCount(), (SCHAR*) m_out_buffer.begin());
	}
	if (status[1]) {
		raise(status, tdbb, "jrd8_fetch");
	}

	return (res != 100);
}


void InternalStatement::doClose(thread_db* tdbb, bool drop)
{
	ISC_STATUS_ARRAY status = {0};
	{
		EngineCallbackGuard guard(tdbb, *this);
		jrd8_free_statement(status, &m_request, drop ? DSQL_drop : DSQL_close);
		m_allocated = (m_request != 0);
	}
	if (status[1])
	{
		m_allocated = m_request = 0;
		raise(status, tdbb, "jrd8_free_statement");
	}
}

void InternalStatement::putExtBlob(thread_db* tdbb, dsc& src, dsc& dst)
{
	if (m_transaction->getScope() == traCommon)
		MOV_move(tdbb, &src, &dst);
	else
		Statement::putExtBlob(tdbb, src, dst);
}

void InternalStatement::getExtBlob(thread_db* tdbb, const dsc& src, dsc& dst)
{
	fb_assert(dst.dsc_length == src.dsc_length);
	fb_assert(dst.dsc_length == sizeof(bid));

	if (m_transaction->getScope() == traCommon)
		memcpy(dst.dsc_address, src.dsc_address, sizeof(bid));
	else
		Statement::getExtBlob(tdbb, src, dst);
}



// InternalBlob

InternalBlob::InternalBlob(InternalConnection& conn) :
	Blob(conn),
	m_connection(conn),
	m_blob(NULL)
{
	m_blob_id.clear();
}

InternalBlob::~InternalBlob()
{
	fb_assert(!m_blob);
}

void InternalBlob::open(thread_db* tdbb, Transaction& tran, const dsc& desc, const UCharBuffer* bpb)
{
	fb_assert(!m_blob);
	fb_assert(sizeof(m_blob_id) == desc.dsc_length);

	Jrd::Attachment* att = m_connection.getJrdAtt();
	jrd_tra* transaction = ((InternalTransaction&) tran).getJrdTran();
	memcpy(&m_blob_id, desc.dsc_address, sizeof(m_blob_id));

	ISC_STATUS_ARRAY status = {0};
	{
		EngineCallbackGuard guard(tdbb, m_connection);

		USHORT bpb_len = bpb ? bpb->getCount() : 0;
		const UCHAR* bpb_buff = bpb ? bpb->begin() : NULL;

		jrd8_open_blob2(status, &att, &transaction, &m_blob, &m_blob_id,
			bpb_len, bpb_buff);
	}
	if (status[1]) {
		m_connection.raise(status, tdbb, "jrd8_open_blob2");
	}
	fb_assert(m_blob);
}

void InternalBlob::create(thread_db* tdbb, Transaction& tran, dsc& desc, const UCharBuffer* bpb)
{
	fb_assert(!m_blob);
	fb_assert(sizeof(m_blob_id) == desc.dsc_length);

	Jrd::Attachment* att = m_connection.getJrdAtt();
	jrd_tra* transaction = ((InternalTransaction&) tran).getJrdTran();
	m_blob_id.clear();

	ISC_STATUS_ARRAY status = {0};
	{
		EngineCallbackGuard guard(tdbb, m_connection);

		const USHORT bpb_len = bpb ? bpb->getCount() : 0;
		const UCHAR* bpb_buff = bpb ? bpb->begin() : NULL;

		jrd8_create_blob2(status, &att, &transaction, &m_blob, &m_blob_id,
			bpb_len, bpb_buff);
		memcpy(desc.dsc_address, &m_blob_id, sizeof(m_blob_id));
	}
	if (status[1]) {
		m_connection.raise(status, tdbb, "jrd8_create_blob2");
	}
	fb_assert(m_blob);
}

USHORT InternalBlob::read(thread_db* tdbb, UCHAR* buff, USHORT len)
{
	fb_assert(m_blob);

	USHORT result = 0;
	ISC_STATUS_ARRAY status = {0};
	{
		EngineCallbackGuard guard(tdbb, m_connection);
		jrd8_get_segment(status, &m_blob, &result, len, buff);
	}
	switch (status[1])
	{
	case isc_segstr_eof:
		fb_assert(result == 0);
		break;
	case isc_segment:
	case 0:
		break;
	default:
		m_connection.raise(status, tdbb, "jrd8_get_segment");
	}

	return result;
}

void InternalBlob::write(thread_db* tdbb, const UCHAR* buff, USHORT len)
{
	fb_assert(m_blob);

	ISC_STATUS_ARRAY status = {0};
	{
		EngineCallbackGuard guard(tdbb, m_connection);
		jrd8_put_segment(status, &m_blob, len, buff);
	}
	if (status[1]) {
		m_connection.raise(status, tdbb, "jrd8_put_segment");
	}
}

void InternalBlob::close(thread_db* tdbb)
{
	fb_assert(m_blob);
	ISC_STATUS_ARRAY status = {0};
	{
		EngineCallbackGuard guard(tdbb, m_connection);
		jrd8_close_blob(status, &m_blob);
	}
	if (status[1]) {
		m_connection.raise(status, tdbb, "jrd8_close_blob");
	}
	fb_assert(!m_blob);
}

void InternalBlob::cancel(thread_db* tdbb)
{
	if (!m_blob) {
		return;
	}

	ISC_STATUS_ARRAY status = {0};
	{
		EngineCallbackGuard guard(tdbb, m_connection);
		jrd8_cancel_blob(status, &m_blob);
	}
	if (status[1]) {
		m_connection.raise(status, tdbb, "jrd8_cancel_blob");
	}
	fb_assert(!m_blob);
}


} // namespace EDS
