/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/***************** gpre version LI-T3.0.0.31129 Firebird 3.0 Alpha 2 **********************/
/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		fun.epp
 *	DESCRIPTION:	External Function handling code.
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
 *
 * 2001.9.18 Claudio Valderrama: Allow return parameter by descriptor
 * to signal NULL by testing the flags of the parameter's descriptor.
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 * 2003.07.31 Fred Polizo, Jr. - Made FUN_evaluate() correctly determine
 * the length of string types containing binary data (char. set octets).
 * 2003.08.10 Claudio Valderrama: Fix SF Bugs #544132 and #728839.
 */

#include "firebird.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../common/config/config.h"
#include "../common/os/path_utils.h"
#include "../jrd/jrd.h"
#include "../jrd/val.h"
#include "../jrd/exe.h"
#include "../jrd/ibase.h"
#include "../jrd/req.h"
#include "../jrd/lls.h"
#include "../jrd/blb.h"
#include "../jrd/flu.h"
#include "../jrd/ibsetjmp.h"
#include "../jrd/irq.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cmp_proto.h"
#include "../common/dsc_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/flu_proto.h"
#include "../jrd/fun_proto.h"
#include "../yvalve/gds_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/thread_proto.h"
#include "../common/isc_s_proto.h"
#include "../jrd/blb.h"
#include "../jrd/ExtEngineManager.h"
#include "../common/classes/auto.h"
#include "../common/utils_proto.h"
#include "../common/classes/FpeControl.h"
#include "../jrd/Function.h"
#include "../jrd/Attachment.h"

#ifdef WIN_NT
#define LIBNAME "ib_util"
#else
#define LIBNAME "libib_util"
#include <dlfcn.h>	// dladdr
#endif

using namespace Jrd;
using namespace Firebird;


namespace
{
	bool volatile initDone = false;

	struct IbUtilStartup
	{
		explicit IbUtilStartup(MemoryPool& p) : libUtilPath(p)
		{
			if (fb_utils::bootBuild())
				return;

#if defined(WIN_NT)
			libUtilPath.assign(LIBNAME);
#elif defined(DARWIN)
			libUtilPath = "/Library/Frameworks/Firebird.framework/Versions/A/Libraries/" LIBNAME;
#else
			const PathName id(Config::getInstallDirectory());
			PathUtils::concatPath(libUtilPath, id, "lib/" LIBNAME);
#endif // WIN_NT
		}

		PathName libUtilPath;
	};

	InitInstance<IbUtilStartup> ibUtilStartup;

	bool tryLibrary(PathName libName, string& message)
	{
		ModuleLoader::doctorModuleExtension(libName);

		ModuleLoader::Module* module = ModuleLoader::loadModule(libName);
		if (!module)
		{
			message.printf("%s library has not been found", libName.c_str());
			return false;
		}

		void (*ibUtilUnit)(void* (*)(long));
		if (!module->findSymbol("ib_util_init", ibUtilUnit))
		{
			message.printf("ib_util_init not found in %s", libName.c_str());
			delete module;
			return false;
		}

		ibUtilUnit(IbUtil::alloc);
		initDone = true;

		return true;
	}
}


void IbUtil::initialize()
{
	if (initDone || fb_utils::bootBuild())
	{
		initDone = true;
		return;
	}

	string message[4];		// To suppress logs when correct library is found

#ifdef WIN_NT
	// using bin folder
	if (tryLibrary(fb_utils::getPrefix(Firebird::DirType::FB_DIR_BIN, LIBNAME), message[1]))
		return;

	// using firebird root (takes into account environment settings)
	if (tryLibrary(fb_utils::getPrefix(Firebird::DirType::FB_DIR_CONF, LIBNAME), message[2]))
		return;
#else
	// using install directory
	if (tryLibrary(ibUtilStartup().libUtilPath, message[0]))
		return;

	// using firebird root (takes into an account environment settings)
	if (tryLibrary(fb_utils::getPrefix(Firebird::DirType::FB_DIR_CONF, "lib/" LIBNAME), message[1]))
		return;

	// using libraries directory
	if (tryLibrary(fb_utils::getPrefix(Firebird::DirType::FB_DIR_LIB, LIBNAME), message[2]))
		return;
#endif // WIN_NT

	// using default paths
	if (tryLibrary(LIBNAME, message[3]))
		return;

	// all failed - log error
	gds__log("ib_util init failed, UDFs can't be used - looks like firebird misconfigured\n"
			 "\t%s\n\t%s\n\t%s\n\t%s", message[0].c_str(), message[1].c_str(),
									   message[2].c_str(), message[3].c_str());
}

void* IbUtil::alloc(long size)
{
	thread_db* tdbb = JRD_get_thread_data();

	void* const ptr = tdbb->getDefaultPool()->allocate(size);

	if (ptr)
		tdbb->getAttachment()->att_udf_pointers.add(ptr);

	return ptr;
}

bool IbUtil::free(void* ptr)
{
	if (!ptr)
		return true;

	thread_db* tdbb = JRD_get_thread_data();
	Jrd::Attachment* attachment = tdbb->getAttachment();

	size_t pos;
	if (attachment->att_udf_pointers.find(ptr, pos))
	{
		attachment->att_udf_pointers.remove(pos);
		tdbb->getDefaultPool()->deallocate(ptr);
		return true;
	}

	return false;
}


typedef void* UDF_ARG;

template <typename T>
T CALL_UDF(Jrd::Attachment* att, int (*entrypoint)(), UDF_ARG* args)
{
	Jrd::Attachment::Checkout attCout(att, FB_FUNCTION);
	return ((T (*)(UDF_ARG, UDF_ARG, UDF_ARG, UDF_ARG, UDF_ARG, UDF_ARG, UDF_ARG, UDF_ARG,
		UDF_ARG, UDF_ARG, UDF_ARG, UDF_ARG, UDF_ARG, UDF_ARG, UDF_ARG))(entrypoint))
		(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7],
		 args[8], args[9], args[10], args[11], args[12], args[13], args[14]);
}

/*DATABASE DB = FILENAME "ODS.RDB";*/


class OwnedBlobStack : public Stack<blb*>
{
public:
	explicit OwnedBlobStack(thread_db* in_tdbb) :
		m_blob_created(0), m_tdbb(in_tdbb)
	{}
	~OwnedBlobStack();
	void close();
	void setBlobCreated(blb* b)
	{
		m_blob_created = b;
	}
private:
	blb* m_blob_created;
	thread_db* m_tdbb;
};

OwnedBlobStack::~OwnedBlobStack()
{
	while (this->hasData())
	{
		// We want to close blobs opened for reading
		// and cancel blobs opened for writing.
		blb* aux = this->pop();
		try
		{
			if (aux != m_blob_created)
				aux->BLB_close(m_tdbb);
			else
				aux->BLB_cancel(m_tdbb);
		}
		catch (const Exception&)
		{
			// Ignore exception.
		}
	}
}

void OwnedBlobStack::close()
{
	while (this->hasData())
	{
		// This strange code is to ensure that read blobs that cannot be closed
		// normally are ignored by the destructor, but if the created (write)
		// blob cannot be closed normally, it will be cancelled by the destructor
		// of OwnedBlobStack.

		blb* aux = this->object();
		if (aux != m_blob_created)
		{
			this->pop();
			aux->BLB_close(m_tdbb);
		}
		else
		{
			aux->BLB_close(m_tdbb);
			this->pop();
		}
	}
}


enum UdfError
{
	UeNone,
	UeUnsupDtype,
	UeMove,
	UeDealloc
};


static void handleUdfException(const Exception& ex);
static void reRaiseExceptionFromUdf(thread_db* tdbb);
static SSHORT blob_get_segment(blb*, UCHAR*, USHORT, USHORT*);
static void blob_put_segment(blb*, const UCHAR*, USHORT);
static SLONG blob_lseek(blb*, USHORT, SLONG);
static SLONG get_scalar_array(const Parameter*, DSC*, scalar_array_desc*, UCharStack&);
static void invoke(thread_db* tdbb,
				   const Function* function,
				   const Parameter* return_ptr,
				   impure_value* value,
				   UDF_ARG* args,
				   const udf_blob* const return_blob_struct,
				   bool& result_is_null,
				   UdfError& udfError);
static bool private_move(Jrd::thread_db* tdbb, dsc* from, dsc* to);


void FUN_evaluate(thread_db* tdbb, const Function* function, const NestValueArray& node,
	impure_value* value)
{
/**************************************
 *
 *	F U N _ e v a l u a t e
 *
 **************************************
 *
 * Functional description
 *	Evaluate a function.
 *
 **************************************/
	// We may be in danger of AV in UDF if ib_util init failed
	if (!initDone)
	{
		ERR_post(Arg::Gds(isc_random) << "ib_util init failed - UDF usage disabled");
	}

	if (!function->fun_entrypoint)
	{
		Firebird::status_exception::raise(Arg::Gds(isc_funnotdef) <<
										  Arg::Str(function->getName().toString()) <<
										  Arg::Gds(isc_modnotfound));
	}

	SET_TDBB(tdbb);

	Array<NestConst<Parameter> > allArgs;

	{	// scope
		const NestConst<Parameter>* end = function->getInputFields().end();
		for (const NestConst<Parameter>* param = function->getInputFields().begin(); param != end; ++param)
		{
			allArgs.resize((*param)->prm_number + 1);
			allArgs[(*param)->prm_number] = *param;
		}

		end = function->getOutputFields().end();
		for (const NestConst<Parameter>* param = function->getOutputFields().begin(); param != end; ++param)
		{
			if ((*param)->prm_number != 0)
			{
				allArgs.resize((*param)->prm_number + 1);
				allArgs[(*param)->prm_number] = *param;
			}
		}
	}

	const Parameter* return_ptr = function->getOutputFields()[0];

	jrd_req* request = tdbb->getRequest();
	// CVC: restoring the null flag seems like a Borland hack to try to
	// patch a bug with null handling. There's no evident reason to restore it
	// because EVL_expr() resets it every time it's called.	Kept it for now.
	const bool null_flag = ((request->req_flags & req_null) == req_null);

	UCharStack array_stack;

	// Trap any potential errors

	try {

	UDF_ARG args[MAX_UDF_ARGUMENTS + 1];
	HalfStaticArray<UCHAR, 800> temp;

	// Start by constructing argument list
	UCHAR* temp_ptr = temp.getBuffer(function->fun_temp_length + FB_DOUBLE_ALIGN);
	MOVE_CLEAR(temp_ptr, temp.getCount());
	temp_ptr = (UCHAR *) FB_ALIGN((U_IPTR) temp_ptr, FB_DOUBLE_ALIGN);

	MOVE_CLEAR(args, sizeof(args));
	UDF_ARG* arg_ptr = args;
	//Stack<blb*> blob_stack;
	//blb* blob_created = 0;
	OwnedBlobStack blob_stack(tdbb);

	const NestConst<ValueExprNode>* ptr = node.begin();

	// We'll use to this trick to give the UDF a way to signal
	// "I sent a null blob" when not using descriptors.
	udf_blob* return_blob_struct = 0;

	DSC temp_desc;
	double d;

	// Process arguments
	const NestConst<Parameter>* const end = allArgs.end();
	for (const NestConst<Parameter>* tail = allArgs.begin(); tail != end; ++tail)
	{
		const Parameter* parameter = *tail;
		if (!parameter)
			continue;

		DSC* input;
		if (*tail == return_ptr)
		{
			input = &value->vlu_desc; // (DSC*) value;
			// CVC: The return param we build for the UDF is not null!!!
			// This closes SF Bug #544132.
			request->req_flags &= ~req_null;
		}
		else
			input = EVL_expr(tdbb, request, *ptr++);

		// If we're passing data type ISC descriptor, there's
		// nothing left to be done

		if (parameter->prm_fun_mechanism == FUN_descriptor)
		{
			// CVC: We have to protect the UDF from Borland's ill null signaling
			// See EVL_expr(...), case nod_field for reference: the request may be
			// signaling NULL, but the placeholder field created by EVL_field(...)
			// doesn't carry the null flag in the descriptor. Why Borland didn't
			// set on such flag is maybe because it only has local meaning.
			// This closes SF Bug #728839.
			if ((request->req_flags & req_null) && !(input && (input->dsc_flags & DSC_null)))
			{
			    *arg_ptr++ = NULL;
			}
			else
			{
			    *arg_ptr++ = input;
			}
			continue;
		}

		temp_desc = parameter->prm_desc;
		temp_desc.dsc_address = temp_ptr;
		// CVC: There's a theoretical possibility of overflowing "length" here.
		USHORT length = FB_ALIGN(temp_desc.dsc_length, FB_DOUBLE_ALIGN);

		// If we've got a null argument, just pass zeros (got any better ideas?)

		if (!input || (request->req_flags & req_null))
		{
			if (parameter->prm_fun_mechanism == FUN_value)
			{
				UCHAR* p = (UCHAR *) arg_ptr;
				MOVE_CLEAR(p, (SLONG) length);
				p += length;
				arg_ptr = reinterpret_cast<UDF_ARG*>(p);
				continue;
			}

			if (parameter->prm_desc.dsc_dtype == dtype_blob)
			{
				length = sizeof(udf_blob);
			}

			if (parameter->prm_fun_mechanism != FUN_ref_with_null)
				MOVE_CLEAR(temp_ptr, (SLONG) length);
			else
			{
				// Probably for arrays and blobs it's better to preserve the
				// current behavior that sends a zeroed memory chunk.
				switch (parameter->prm_desc.dsc_dtype)
				{
				case dtype_quad:
				case dtype_array:
				case dtype_blob:
					MOVE_CLEAR(temp_ptr, (SLONG) length);
					break;
				default: // FUN_ref_with_null, non-blob, non-array: we send null pointer.
					*arg_ptr++ = 0;
					continue;
				}
			}
		}
		else if (parameter->prm_fun_mechanism == FUN_scalar_array)
		{
			length = get_scalar_array(parameter, input, (scalar_array_desc*)temp_ptr, array_stack);
		}
		else
		{
			SLONG l;
			SLONG* lp;
			switch (parameter->prm_desc.dsc_dtype)
			{
			case dtype_short:
				{
					const SSHORT s = MOV_get_long(input, (SSHORT) parameter->prm_desc.dsc_scale);
					if (parameter->prm_fun_mechanism == FUN_value)
					{
						// For (apparent) portability reasons, SHORT by value
						// is always passed as a LONG.  See v3.2 release notes
						// Passing by value is not supported in SQL due to
						// these problems, but can still occur in GDML.
						// 1994-September-28 David Schnepper

						*arg_ptr++ = (UDF_ARG)(IPTR) s;
						continue;
					}

					SSHORT* sp = (SSHORT*) temp_ptr;
					*sp = s;
				}
				break;

			case dtype_long:
				l = MOV_get_long(input, (SSHORT) parameter->prm_desc.dsc_scale);
				if (parameter->prm_fun_mechanism == FUN_value)
				{
					*arg_ptr++ = (UDF_ARG)(IPTR) l;
					continue;
				}
				lp = (SLONG *) temp_ptr;
				*lp = l;
				break;

			case dtype_sql_time:
				l = MOV_get_sql_time(input);
				if (parameter->prm_fun_mechanism == FUN_value)
				{
					*arg_ptr++ = (UDF_ARG)(IPTR) l;
					continue;
				}
				lp = (SLONG *) temp_ptr;
				*lp = l;
				break;

			case dtype_sql_date:
				l = MOV_get_sql_date(input);
				if (parameter->prm_fun_mechanism == FUN_value)
				{
					*arg_ptr++ = (UDF_ARG)(IPTR) l;
					continue;
				}
				lp = (SLONG *) temp_ptr;
				*lp = l;
				break;

			case dtype_int64:
				{
					SINT64* pi64;
					const SINT64 i64 = MOV_get_int64(input, (SSHORT) parameter->prm_desc.dsc_scale);

					if (parameter->prm_fun_mechanism == FUN_value)
					{
						pi64 = (SINT64*) arg_ptr;
						*pi64++ = i64;
						arg_ptr = reinterpret_cast<UDF_ARG*>(pi64);
						continue;
					}
					pi64 = (SINT64*) temp_ptr;
					*pi64 = i64;
				}
				break;

			case dtype_real:
				{
					const float f = (float) MOV_get_double(input);
					if (parameter->prm_fun_mechanism == FUN_value)
					{
						// For (apparent) portability reasons, FLOAT by value
						// is always passed as a DOUBLE.  See v3.2 release notes
						// Passing by value is not supported in SQL due to
						// these problems, but can still occur in GDML.
						// 1994-September-28 David Schnepper

						double* dp = (double *) arg_ptr;
						*dp++ = (double) f;
						arg_ptr = reinterpret_cast<UDF_ARG*>(dp);
						continue;
					}
					float* fp = (float *) temp_ptr;
					*fp = f;
				}
				break;

			case dtype_double:
				d = MOV_get_double(input);
				double* dp;
				if (parameter->prm_fun_mechanism == FUN_value)
				{
					dp = (double*) arg_ptr;
					*dp++ = d;
					arg_ptr = reinterpret_cast<UDF_ARG*>(dp);
					continue;
				}
				dp = (double*) temp_ptr;
				*dp = d;
				break;

			case dtype_text:
			case dtype_dbkey:
			case dtype_cstring:
			case dtype_varying:
				if (*tail == return_ptr)
				{
					//temp_ptr = value->vlu_desc.dsc_address;
					//length = 0;
					*arg_ptr++ = value->vlu_desc.dsc_address;
					continue;
				}

				MOV_move(tdbb, input, &temp_desc);
				break;

			// CVC: There's no other solution for now: timestamp can't be returned
			//		by value and the other way is to force the user to pass a dummy value as
			//		an argument to keep the engine happy. So, here's the hack.
			case dtype_timestamp:
			    if (*tail == return_ptr)
				{
					//temp_ptr = value->vlu_desc.dsc_address;
					//length = sizeof(GDS_TIMESTAMP);
					*arg_ptr++ = value->vlu_desc.dsc_address;
					continue;
				}

				MOV_move(tdbb, input, &temp_desc);
				break;

			case dtype_quad:
			case dtype_array:
				MOV_move(tdbb, input, &temp_desc);
				break;

			case dtype_blob:
				{
					// This is not a descriptor pointing to a blob. This is a blob struct.
					udf_blob* blob_desc = (udf_blob*) temp_ptr;
					blb* blob;
					length = sizeof(udf_blob);
					if (*tail == return_ptr)
					{
						blob = blb::create(tdbb, tdbb->getRequest()->req_transaction,
										  (bid*) &value->vlu_misc);
						return_blob_struct = blob_desc;
						blob_stack.setBlobCreated(blob);
					}
					else
					{
						bid blob_id;
						if (request->req_flags & req_null)
						{
							memset(&blob_id, 0, sizeof(bid));
						}
						else
						{
							if (input->dsc_dtype != dtype_quad && input->dsc_dtype != dtype_blob)
							{
								ERR_post(Arg::Gds(isc_wish_list) <<
										 Arg::Gds(isc_blobnotsup) << Arg::Str("conversion"));
							}
							blob_id = *(bid*) input->dsc_address;
						}
						blob = blb::open(tdbb, tdbb->getRequest()->req_transaction, &blob_id);
					}
					blob_stack.push(blob);
					blob_desc->blob_get_segment = blob_get_segment;
					blob_desc->blob_put_segment = blob_put_segment;
					blob_desc->blob_seek = blob_lseek;
					blob_desc->blob_handle = blob;
					blob_desc->blob_number_segments = blob->getSegmentCount();
					blob_desc->blob_max_segment = blob->getMaxSegment();
					blob_desc->blob_total_length = blob->blb_length;
				}
				break;

			case dtype_boolean:
				l = (SLONG) MOV_get_boolean(input);
				if (parameter->prm_fun_mechanism == FUN_value)
				{
					*arg_ptr++ = (UDF_ARG)(IPTR) l;
					continue;
				}
				lp = (SLONG*) temp_ptr;
				*lp = l;
				break;

			default:
				fb_assert(FALSE);
				MOV_move(tdbb, input, &temp_desc);
				break;

			}
		}

		*arg_ptr++ = temp_ptr;
		temp_ptr += length;
	} // for

	// Did the udf manage to signal null in some way?
	// We acknowledge null in three cases:
	// a) rc_ptr = udf(); returns a null pointer -> result_was_null becomes true
	// b) Udf used RETURNS PARAMETER <n> for a descriptor whose DSC_null flag is activated
	// c) Udf used RETURNS PARAMETER <n> for a blob and made the blob handle null,
	// because there's no current way to do that. Notice that it doesn't affect
	// the engine internals, since blob_struct is a mere wrapper around the blob.
	// Udfs work in the assumption that they ignore that the handle is the real
	// internal blob and this has been always the tale.
	bool result_was_null = false;

	// Did the udf send an unknown data type or one we can't handle?
	// Did the udf mismanage memory marked with FREE_IT,
	// that should be deallocated by the engine?
	// Did the udf send an ill-formed descriptor back?
	UdfError udfError = UeNone;

	invoke(tdbb, function, return_ptr, value, args, return_blob_struct, result_was_null, udfError);

	switch (udfError)
	{
	case UeUnsupDtype:
		IBERROR(169);			// msg 169 return data type not supported
		break;
	case UeMove:
		//CVT_conversion_error(&desc, ERR_post);
		ERR_punt(); // The error is already in the thread's tdbb_status_vector
		break;
	case UeDealloc:
		status_exception::raise(Arg::Gds(isc_bad_udf_freeit));
		break;
	}

	if (result_was_null)
	{
		request->req_flags |= req_null;
		value->vlu_desc.dsc_flags |= DSC_null; // redundant, but be safe
	}
	else
	{
		if (value->vlu_desc.dsc_dtype == dtype_double)
		{
			if (isinf(value->vlu_misc.vlu_double))
			{
				status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
									Arg::Gds(isc_udf_fp_overflow) <<
									Arg::Str(function->getName().toString()));
			}
			else if (isnan(value->vlu_misc.vlu_double))
			{
				status_exception::raise(Arg::Gds(isc_expression_eval_err) <<
									Arg::Gds(isc_udf_fp_nan) <<
									Arg::Str(function->getName().toString()));
			}
		}
		request->req_flags &= ~req_null;
	}

	while (array_stack.hasData())
	{
		delete[] array_stack.pop();
	}
	blob_stack.close();

	} // try
	catch (const Exception&)
	{
		while (array_stack.hasData()) {
			delete[] array_stack.pop();
		}

		throw;
	}

	if (null_flag)
	{
		request->req_flags |= req_null;
	}
}


static void handleUdfException(const Exception& ex)
{
/****************************************
 *
 *	h a n d l e U d f E x c e p t i o n
 *
 ****************************************
 *
 * Functional description
 *	When exception happens in UDF we have
 *	no better choice than store it in
 *	tdbb_status_vector and raise again
 *	on return from UDF.
 *
 ****************************************/
	thread_db* tdbb = JRD_get_thread_data();
	fb_utils::init_status(tdbb->tdbb_status_vector);
	ex.stuff_exception(tdbb->tdbb_status_vector);
}


static void reRaiseExceptionFromUdf(thread_db* tdbb)
{
/*************************************************
 *
 *	r e R a i s e E x c e p t i o n F r o m U d f
 *
 *************************************************
 *
 * Functional description
 *	When exception happens in UDF we have
 *	no better choice than store it in
 *	tdbb_status_vector and raise again
 *	on return from UDF.
 *
 *************************************************/
	ISC_STATUS* st = tdbb->tdbb_status_vector;
	if (st[0] == 1 && st[1] != 0)
	{
		ERR_punt();
	}
}


static SLONG blob_lseek(blb* blob, USHORT mode, SLONG offset)
{
/**************************************
 *
 *	b l o b _ l s e e k
 *
 **************************************
 *
 * Functional description
 *	lseek a blob segement.  Return the offset
 *
 **************************************/
	// As this is a call-back from a UDF, must put it under try/catch and reacquire the engine mutex
	try
	{
		thread_db* tdbb = JRD_get_thread_data();
		Jrd::Attachment::SyncGuard guard(tdbb->getAttachment(), FB_FUNCTION);

		return blob->BLB_lseek(mode, offset);
	}
	catch (const Exception& ex)
	{
		handleUdfException(ex);
		return -1;
	}
}


static void blob_put_segment(blb* blob, const UCHAR* buffer, USHORT length)
{
/**************************************
 *
 *	b l o b _ p u t _ s e g m e n t
 *
 **************************************
 *
 * Functional description
 *	Put segment into a blob.  Return nothing
 *
 **************************************/
	// As this is a call-back from a UDF, must put it under try/catch and reacquire the engine mutex
	try
	{
		thread_db* tdbb = JRD_get_thread_data();
		Jrd::Attachment::SyncGuard guard(tdbb->getAttachment(), FB_FUNCTION);

		blob->BLB_put_segment(tdbb, buffer, length);
	}
	catch (const Exception& ex)
	{
		handleUdfException(ex);
	}
}


static SSHORT blob_get_segment(blb* blob, UCHAR* buffer, USHORT length, USHORT* return_length)
{
/**************************************
 *
 *	b l o b _ g e t _ s e g m e n t
 *
 **************************************
 *
 * Functional description
 *	Get next segment of a blob.  Return the following:
 *
 *		1	-- Complete segment has been returned.
 *		0	-- End of blob (no data returned).
 *		-1	-- Current segment is incomplete.
 *
 **************************************/
	// As this is a call-back from a UDF, must put it under try/catch and reacquire the engine mutex
	try
	{
		thread_db* tdbb = JRD_get_thread_data();
		Jrd::Attachment::SyncGuard guard(tdbb->getAttachment(), FB_FUNCTION);

		*return_length = blob->BLB_get_segment(tdbb, buffer, length);

		if (blob->blb_flags & BLB_eof)
			return 0;

		if (blob->getFragmentSize())
			return -1;

		return 1;
	}
	catch (const Exception& ex)
	{
		handleUdfException(ex);
		return 0;
	}
}


static SLONG get_scalar_array(const Parameter* arg,
							  DSC*	value,
							  scalar_array_desc*	scalar_desc,
							  UCharStack&	stack)
{
/**************************************
 *
 *	g e t _ s c a l a r _ a r r a y
 *
 **************************************
 *
 * Functional description
 *	Get and format a scalar array descriptor, then allocate space
 *	and fetch the array.  If conversion is required, convert.
 *	Return length of array desc.
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();

	// Get first the array descriptor, then the array

	SLONG stuff[IAD_LEN(16) / 4];
	Ods::InternalArrayDesc* array_desc = (Ods::InternalArrayDesc*) stuff;
	blb* blob = blb::get_array(tdbb, tdbb->getRequest()->req_transaction, (bid*)value->dsc_address,
							  array_desc);

	fb_assert(array_desc->iad_total_length >= 0); // check before upcasting to size_t
	UCHAR* data = FB_NEW(*getDefaultMemoryPool()) UCHAR[static_cast<size_t>(array_desc->iad_total_length)];
	blob->BLB_get_data(tdbb, data, array_desc->iad_total_length);
	const USHORT dimensions = array_desc->iad_dimensions;

	// Convert array, if necessary

	dsc to = arg->prm_desc;
	dsc from = array_desc->iad_rpt[0].iad_desc;

	if (to.dsc_dtype != from.dsc_dtype ||
		to.dsc_scale != from.dsc_scale || to.dsc_length != from.dsc_length)
	{
		SLONG n = array_desc->iad_count;
		UCHAR* const temp = FB_NEW(*getDefaultMemoryPool()) UCHAR[static_cast<size_t>(to.dsc_length * n)];
		to.dsc_address = temp;
		from.dsc_address = data;

		// This loop may call ERR_post indirectly.
		try
		{
			for (; n; --n, to.dsc_address += to.dsc_length,
				from.dsc_address += array_desc->iad_element_length)
			{
				MOV_move(tdbb, &from, &to);
			}
		}
		catch (const Exception&)
		{
			delete[] data;
			delete[] temp;
			throw;
		}

		delete[] data;
		data = temp;
	}

	// Fill out the scalar array descriptor

	stack.push(data);
	scalar_desc->sad_desc = arg->prm_desc;
	scalar_desc->sad_desc.dsc_address = data;
	scalar_desc->sad_dimensions = dimensions;

	const Ods::InternalArrayDesc::iad_repeat* tail1 = array_desc->iad_rpt;
	scalar_array_desc::sad_repeat* tail2 = scalar_desc->sad_rpt;
	for (const scalar_array_desc::sad_repeat* const end = tail2 + dimensions; tail2 < end;
		++tail1, ++tail2)
	{
		tail2->sad_upper = tail1->iad_upper;
		tail2->sad_lower = tail1->iad_lower;
	}

	return static_cast<SLONG>(sizeof(scalar_array_desc) + (dimensions - 1u) * sizeof(scalar_array_desc::sad_repeat));
}


static void invoke(thread_db* tdbb,
				   const Function* function,
				   const Parameter* return_ptr,
				   impure_value* value,
				   UDF_ARG* args,
				   const udf_blob* const return_blob_struct,
				   bool& result_is_null,
				   UdfError& udfError)
{
/**************************************
 *
 *	i n v o k e
 *
 **************************************
 *
 * Functional description
 *	Real UDF call moved to separate function in order to
 *	use CHECK_FOR_EXCEPTIONS macros without conflicts with destructors
 *
 **************************************/
	SET_TDBB(tdbb);
	Jrd::Attachment* att = tdbb->getAttachment();

	// Get ready to accept exceptions from API called from UDF
	fb_utils::init_status(tdbb->tdbb_status_vector);

	START_CHECK_FOR_EXCEPTIONS(function->fun_exception_message.c_str());
	if (function->fun_return_arg)
	{
		CALL_UDF<void>(att, function->fun_entrypoint, args);
		result_is_null = return_ptr->prm_fun_mechanism == FUN_descriptor &&
			(value->vlu_desc.dsc_flags & DSC_null) ||
			return_ptr->prm_fun_mechanism == FUN_blob_struct && return_blob_struct &&
			!return_blob_struct->blob_handle;
	}
	else if (return_ptr->prm_fun_mechanism == FUN_value)
	{
		result_is_null = false;

		switch (value->vlu_desc.dsc_dtype)
		{
		case dtype_sql_time:
		case dtype_sql_date:
		case dtype_long:
			value->vlu_misc.vlu_long = CALL_UDF<SLONG>(att, function->fun_entrypoint, args);
			break;

		case dtype_short:
			// For (apparent) portability reasons, SHORT by value
			// must always be returned as a LONG.  See v3.2 release notes
			// 1994-September-28 David Schnepper

			value->vlu_misc.vlu_short = (SSHORT) CALL_UDF<SLONG>(att, function->fun_entrypoint, args);
			break;

		case dtype_real:
			// For (apparent) portability reasons, FLOAT by value
			// must always be returned as a DOUBLE.  See v3.2 release notes
			// 1994-September-28 David Schnepper

			value->vlu_misc.vlu_float = (float) CALL_UDF<double>(att, function->fun_entrypoint, args);
			break;

		case dtype_int64:
			value->vlu_misc.vlu_int64 = CALL_UDF<SINT64>(att, function->fun_entrypoint, args);
			break;

		case dtype_double:
			value->vlu_misc.vlu_double = CALL_UDF<double>(att, function->fun_entrypoint, args);
			break;

		case dtype_timestamp:
		default:
			udfError = UeUnsupDtype;
			break;
		}
	}
	else
	{
		UCHAR* temp_ptr = CALL_UDF<UCHAR*>(att, function->fun_entrypoint, args);

		if (temp_ptr != NULL)
		{
			// CVC: Allow processing of return by descriptor.
			//		If the user did modify the return type, then we'll try to
			//		convert it to the declared return type of the UDF.
			dsc* return_dsc = 0;
			result_is_null = false;
			if ((FUN_T) abs(return_ptr->prm_fun_mechanism) == FUN_descriptor)
			{
				// The formal param's type is contained in value->vlu_desc.dsc_dtype
				// but I want to know if the UDF changed it to a compatible type
				// from its returned descriptor, that will be return_dsc.
				return_dsc = reinterpret_cast<dsc*>(temp_ptr);
				temp_ptr = return_dsc->dsc_address;
				if (!temp_ptr || (return_dsc->dsc_flags & DSC_null))
					result_is_null = true;
			}

			const bool must_free = (SLONG) return_ptr->prm_fun_mechanism < 0;

			if (result_is_null)
				; // nothing to do
			else
			if (return_dsc)
			{
				if (!private_move(tdbb, return_dsc, &value->vlu_desc))
					udfError = UeMove;
			}
			else
			{
				DSC temp_desc;

				switch (value->vlu_desc.dsc_dtype)
				{
				case dtype_sql_date:
				case dtype_sql_time:
					value->vlu_misc.vlu_long = *(SLONG *) temp_ptr;
					break;

				case dtype_long:
					value->vlu_misc.vlu_long = *(SLONG *) temp_ptr;
					break;

				case dtype_short:
					value->vlu_misc.vlu_short = *(SSHORT *) temp_ptr;
					break;

				case dtype_real:
					value->vlu_misc.vlu_float = *(float *) temp_ptr;
					break;

				case dtype_int64:
					value->vlu_misc.vlu_int64 = *(SINT64 *) temp_ptr;
					break;

				case dtype_double:
					value->vlu_misc.vlu_double = *(double *) temp_ptr;
					break;

				case dtype_text:
					temp_desc = value->vlu_desc;
					temp_desc.dsc_address = temp_ptr;
					if (!private_move(tdbb, &temp_desc, &value->vlu_desc))
						udfError = UeMove;
					break;

				case dtype_cstring:
					// For the ttype_binary char. set, this will truncate
					// the string after the first zero octet copied.
					temp_desc = value->vlu_desc;
					temp_desc.dsc_address = temp_ptr;
					temp_desc.dsc_length = static_cast<USHORT>(strlen(reinterpret_cast<char*>(temp_ptr)) + 1);
					if (!private_move(tdbb, &temp_desc, &value->vlu_desc))
						udfError = UeMove;
					break;

				case dtype_varying:
					temp_desc = value->vlu_desc;
					temp_desc.dsc_address = temp_ptr;
					temp_desc.dsc_length = static_cast<USHORT>(reinterpret_cast<vary*>(temp_ptr)->vary_length + sizeof(USHORT));
					if (!private_move(tdbb, &temp_desc, &value->vlu_desc))
						udfError = UeMove;
					break;

				case dtype_timestamp:
					{
						const SLONG* ip = (SLONG *) temp_ptr;
						value->vlu_misc.vlu_dbkey[0] = *ip++;
						value->vlu_misc.vlu_dbkey[1] = *ip;
					}
					break;

				default:
					udfError = UeUnsupDtype;
					break;
				}
			}

			// Check if this is function has the FREE_IT set, if set and
			// return_value is not null, then free the return value.
			if (must_free)
			{
				if (temp_ptr && !IbUtil::free(temp_ptr) && udfError == UeNone)
					udfError = UeDealloc;

				// CVC: Let's free the descriptor, too.
				if (return_dsc && !IbUtil::free(return_dsc) && udfError == UeNone)
					udfError = UeDealloc;
			}
		}
		else
			result_is_null = true;
	}
	END_CHECK_FOR_EXCEPTIONS(function->fun_exception_message.c_str());

	reRaiseExceptionFromUdf(tdbb);
}


static bool private_move(Jrd::thread_db* tdbb, dsc* from, dsc* to)
{
	SET_TDBB(tdbb);

	try
	{
		ThreadStatusGuard tempStatus(tdbb);
		MOV_move(tdbb, from, to);
		return true;
	}
	catch (Firebird::status_exception& e)
	{
		e.stuff_exception(tdbb->tdbb_status_vector);
		return false;
	}
}