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
 *  The Original Code was created by Mark O'Donohue
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2002 Mark O'Donohue <skywalker@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *  2005.05.19 Claudio Valderrama: signal tokens that aren't reserved in the
 *      engine thanks to special handling.
 *  Adriano dos Santos Fernandes
 */

#include "firebird.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#define _yacc_defines_yystype
#include "gen/parse.h"
#include "keywords.h"

// CVC: The latest column indicates whether the token has special handling in
// the parser. If it does, KEYWORD_stringIsAToken will return false.
// I discovered isql was being fooled and put double quotes around those
// special cases unnecessarily.

static const TOK tokens[] =
{
	{NOT_LSS, "!<", 1, false},
	{NEQ, "!=", 1, false},
	{NOT_GTR, "!>", 1, false},
	{LPAREN, "(", 1, false},
	{RPAREN, ")", 1, false},
	{COMMA, ",", 1, false},
	{LSS, "<", 1, false},
	{LEQ, "<=", 1, false},
	{NEQ, "<>", 1, false},	// Alias of !=
	{EQL, "=", 1, false},
	{GTR, ">", 1, false},
	{GEQ, ">=", 1, false},
	{BIND_PARAM, ":=", 2, false},
	{ABS, "ABS", 2, false},
	{KW_ABSOLUTE, "ABSOLUTE", 2, true},
	{ACCENT, "ACCENT", 2, true},
	{ACOS, "ACOS", 2, false},
	{ACOSH, "ACOSH", 2, false},
	{ACTION, "ACTION", 1, true},
	{ACTIVE, "ACTIVE", 1, false},
	{ADD, "ADD", 1, false},
	{ADMIN, "ADMIN", 1, false},
	{AFTER, "AFTER", 1, false},
	{ALL, "ALL", 1, false},
	{ALTER, "ALTER", 1, false},
	{ALWAYS, "ALWAYS", 2, true},
	{AND, "AND", 1, false},
	{ANY, "ANY", 1, false},
	{AS, "AS", 1, false},
	{ASC, "ASC", 1, false},	// Alias of ASCENDING
	{ASC, "ASCENDING", 1, false},
	{ASCII_CHAR, "ASCII_CHAR", 2, false},
	{ASCII_VAL, "ASCII_VAL", 2, false},
	{ASIN, "ASIN", 2, false},
	{ASINH, "ASINH", 2, false},
	{AT, "AT", 1, false},
	{ATAN, "ATAN", 2, false},
	{ATAN2, "ATAN2", 2, false},
	{ATANH, "ATANH", 2, false},
	{AUTO, "AUTO", 1, false},
	{AUTONOMOUS, "AUTONOMOUS", 2, false},
	{AVG, "AVG", 1, false},
	{BACKUP, "BACKUP", 2, true},
	{BEFORE, "BEFORE", 1, false},
	{BEGIN, "BEGIN", 1, false},
	{BETWEEN, "BETWEEN", 1, false},
	{BIGINT, "BIGINT", 2, false},
	{BIN_AND, "BIN_AND", 2, false},
	{BIN_NOT, "BIN_NOT", 2, false},
	{BIN_OR, "BIN_OR", 2, false},
	{BIN_SHL, "BIN_SHL", 2, false},
	{BIN_SHR, "BIN_SHR", 2, false},
	{BIN_XOR, "BIN_XOR", 2, false},
	{BIT_LENGTH, "BIT_LENGTH", 2, false},
	{BLOB, "BLOB", 1, false},
	{BLOCK, "BLOCK", 2, true},
	{BODY, "BODY", 2, true},
	{KW_BOOLEAN, "BOOLEAN", 2, false},
	{BOTH, "BOTH", 2, false},
	{KW_BREAK, "BREAK", 2, true},
	{BY, "BY", 1, false},
	{CALLER, "CALLER", 2, true},
	{CASCADE, "CASCADE", 1, true},
	{CASE, "CASE", 2, false},
	{CAST, "CAST", 1, false},
	{CEIL, "CEIL", 2, false},	// alias of CEILING
	{CEIL, "CEILING", 2, false},
	{KW_CHAR, "CHAR", 1, false},
	{CHAR_LENGTH, "CHAR_LENGTH", 2, false},
	{CHAR_TO_UUID, "CHAR_TO_UUID", 2, false},
	{CHARACTER, "CHARACTER", 1, false},
	{CHARACTER_LENGTH, "CHARACTER_LENGTH", 2, false},
	{CHECK, "CHECK", 1, false},
	{CLOSE, "CLOSE", 2, false},
	{COALESCE, "COALESCE", 2, true},
	{COLLATE, "COLLATE", 1, false},
	{COLLATION, "COLLATION", 2, true},
	{COLUMN, "COLUMN", 2, false},
	{COMMENT, "COMMENT", 2, true},
	{COMMIT, "COMMIT", 1, false},
	{COMMITTED, "COMMITTED", 1, false},
	{COMMON, "COMMON", 2, true},
	{COMPUTED, "COMPUTED", 1, false},
	{CONDITIONAL, "CONDITIONAL", 1, false},
	{CONNECT, "CONNECT", 2, false},
	{CONSTRAINT, "CONSTRAINT", 1, false},
	{CONTAINING, "CONTAINING", 1, false},
	{CONTINUE, "CONTINUE", 2, true},
	{COS, "COS", 2, false},
	{COSH, "COSH", 2, false},
	{COT, "COT", 2, false},
	{COUNT, "COUNT", 1, false},
	{CREATE, "CREATE", 1, false},
	{CROSS, "CROSS", 2, false},
	{CSTRING, "CSTRING", 1, false},
	{CURRENT, "CURRENT", 1, false},
	{CURRENT_CONNECTION, "CURRENT_CONNECTION", 2, false},
	{CURRENT_DATE, "CURRENT_DATE", 2, false},
	{CURRENT_ROLE, "CURRENT_ROLE", 2, false},
	{CURRENT_TIME, "CURRENT_TIME", 2, false},
	{CURRENT_TIMESTAMP, "CURRENT_TIMESTAMP", 2, false},
	{CURRENT_TRANSACTION, "CURRENT_TRANSACTION", 2, false},
	{CURRENT_USER, "CURRENT_USER", 2, false},
	{CURSOR, "CURSOR", 1, false},
	{DATABASE, "DATABASE", 1, false},
	{DATA, "DATA", 2, true},
	{DATE, "DATE", 1, false},
	{DATEADD, "DATEADD", 2, false},
	{DATEDIFF, "DATEDIFF", 2, false},
	{DAY, "DAY", 2, false},
	{DDL, "DDL", 2, false},
	{KW_DEC, "DEC", 1, false},
	{DECIMAL, "DECIMAL", 1, false},
	{DECLARE, "DECLARE", 1, false},
	{DECODE, "DECODE", 2, false},
	{DEFAULT, "DEFAULT", 1, false},
	{KW_DELETE, "DELETE", 1, false},
	{DELETING, "DELETING", 2, true},
	{DENSE_RANK, "DENSE_RANK", 2, false},
	{DESC, "DESC", 1, false},	// Alias of DESCENDING
	{DESC, "DESCENDING", 1, false},
	{KW_DESCRIPTOR,	"DESCRIPTOR", 2, true},
	{DETERMINISTIC, "DETERMINISTIC", 2, false},
	{KW_DIFFERENCE, "DIFFERENCE", 2, true},
	{DISCONNECT, "DISCONNECT", 2, false},
	{DISTINCT, "DISTINCT", 1, false},
	{DO, "DO", 1, false},
	{DOMAIN, "DOMAIN", 1, false},
	{KW_DOUBLE, "DOUBLE", 1, false},
	{DROP, "DROP", 1, false},
	{ELSE, "ELSE", 1, false},
	{END, "END", 1, false},
	{ENGINE, "ENGINE", 2, true},
	{ENTRY_POINT, "ENTRY_POINT", 1, false},
	{ESCAPE, "ESCAPE", 1, false},
	{EXCEPTION, "EXCEPTION", 1, false},
	{EXECUTE, "EXECUTE", 1, false},
	{EXISTS, "EXISTS", 1, false},
	{EXIT, "EXIT", 1, false},
	{EXP, "EXP", 2, false},
	{EXTERNAL, "EXTERNAL", 1, false},
	{EXTRACT, "EXTRACT", 2, false},
	{KW_FALSE, "FALSE", 2, false},
	{FETCH, "FETCH", 2, false},
	{KW_FILE, "FILE", 1, false},
	{FILTER, "FILTER", 1, false},
	{FIRST, "FIRST", 2, true},
	{FIRST_VALUE, "FIRST_VALUE", 2, false},
	{FIRSTNAME, "FIRSTNAME", 2, false},
	{KW_FLOAT, "FLOAT", 1, false},
	{FLOOR, "FLOOR", 2, false},
	{FOR, "FOR", 1, false},
	{FOREIGN, "FOREIGN", 1, false},
	{FREE_IT, "FREE_IT", 1, true},
	{FROM, "FROM", 1, false},
	{FULL, "FULL", 1, false},
	{FUNCTION, "FUNCTION", 1, false},
	{GDSCODE, "GDSCODE", 1, false},
	{GENERATED, "GENERATED", 2, true},
	{GENERATOR, "GENERATOR", 1, false},
	{GEN_ID, "GEN_ID", 1, false},
	{GEN_UUID, "GEN_UUID", 2, false},
	{GLOBAL, "GLOBAL", 2, false},
	{GRANT, "GRANT", 1, false},
	{GRANTED, "GRANTED", 2, false},
	{GROUP, "GROUP", 1, false},
	{HASH, "HASH", 2, false},
	{HAVING, "HAVING", 1, false},
	{HOUR, "HOUR", 2, false},
	{IDENTITY, "IDENTITY", 2, false},
	{IF, "IF", 1, false},
	{KW_IGNORE, "IGNORE", 2, true},
	{IIF, "IIF", 2, true},
	{KW_IN, "IN", 1, false},
	{INACTIVE, "INACTIVE", 1, false},
	{INDEX, "INDEX", 1, false},
	{INNER, "INNER", 1, false},
	{INPUT_TYPE, "INPUT_TYPE", 1, false},
	{INSENSITIVE, "INSENSITIVE", 2, false},
	{INSERT, "INSERT", 1, false},
	{INSERTING, "INSERTING", 2, true},
	{KW_INT, "INT", 1, false},
	{INTEGER, "INTEGER", 1, false},
	{INTO, "INTO", 1, false},
	{IS, "IS", 1, false},
	{ISOLATION, "ISOLATION", 1, false},
	{JOIN, "JOIN", 1, false},
	{KEY, "KEY", 1, false},
	{LAG, "LAG", 2, false},
	{LAST, "LAST", 2, true},
	{LASTNAME, "LASTNAME", 2, false},
	{LEAD, "LEAD", 2, false},
	{LEADING, "LEADING", 2, false},
	{LEAVE, "LEAVE", 2, true},
	{LEFT, "LEFT", 1, false},
	{LENGTH, "LENGTH", 1, false},
	{LEVEL, "LEVEL", 1, false},
	{LIKE, "LIKE", 1, false},
	{LIMBO, "LIMBO", 2, true},
	{LIST, "LIST", 2, false},
	{LN, "LN", 2, false},
	{LOCK, "LOCK", 2, true},
	{LOG, "LOG", 2, false},
	{LOG10, "LOG10", 2, false},
	{KW_LONG, "LONG", 1, false},
	{KW_LOWER, "LOWER", 2, false},
	{LPAD, "LPAD", 2, false},
	{MANUAL, "MANUAL", 1, false},
	{MAPPING, "MAPPING", 2, false},
	{MATCHED, "MATCHED", 2, false},
	{MATCHING, "MATCHING", 2, false},
	{MAXIMUM, "MAX", 1, false},
	{MAXVALUE, "MAXVALUE", 2, false},
	{MAX_SEGMENT, "MAXIMUM_SEGMENT", 1, false},
	{MERGE, "MERGE", 1, false},
	{MILLISECOND, "MILLISECOND", 2, false},
	{MIDDLENAME, "MIDDLENAME", 2, false},
	{MINIMUM, "MIN", 1, false},
	{MINUTE, "MINUTE", 2, false},
	{MINVALUE, "MINVALUE", 2, false},
	{MOD, "MOD", 2, false},
	{MODULE_NAME, "MODULE_NAME", 1, false},
	{MONTH, "MONTH", 2, false},
	{NAME, "NAME", 2, true},
	{NAMES, "NAMES", 1, false},
	{NATIONAL, "NATIONAL", 1, false},
	{NATURAL, "NATURAL", 1, false},
	{NCHAR, "NCHAR", 1, false},
	{NEXT, "NEXT", 2, true},
	{NO, "NO", 1, false},
	{NOT, "NOT", 1, false},
	{NULLIF, "NULLIF", 2, true},
	{KW_NULL, "NULL", 1, false},
	{NULLS, "NULLS", 2, true},
	{KW_NUMERIC, "NUMERIC", 1, false},
	{OCTET_LENGTH, "OCTET_LENGTH", 2, false},
	{OF, "OF", 1, false},
	{ON, "ON", 1, false},
	{ONLY, "ONLY", 1, false},
	{OPEN, "OPEN", 2, false},
	{OPTION, "OPTION", 1, false},
	{OR, "OR", 1, false},
	{ORDER, "ORDER", 1, false},
	{OS_NAME, "OS_NAME", 2, false},
	{OUTER, "OUTER", 1, false},
	{OUTPUT_TYPE, "OUTPUT_TYPE", 1, false},
	{OVER, "OVER", 2, false},
	{OVERFLOW, "OVERFLOW", 1, false},
	{OVERLAY, "OVERLAY", 2, false},
	{PACKAGE, "PACKAGE", 2, true},
	{PAD, "PAD", 2, true},
	{PAGE, "PAGE", 1, false},
	{PAGES, "PAGES", 1, false},
	{KW_PAGE_SIZE, "PAGE_SIZE", 1, false},
	{PARAMETER, "PARAMETER", 1, false},
	{PARTITION, "PARTITION", 2, false},
	{PASSWORD, "PASSWORD", 1, false},
	{PI, "PI", 2, false},
	{PLACING, "PLACING", 2, false},
	{PLAN, "PLAN", 1, false},
	{POSITION, "POSITION", 1, false},
	{POST_EVENT, "POST_EVENT", 1, false},
	{POWER, "POWER", 2, false},
	{PRECISION, "PRECISION", 1, false},
	{PRESERVE, "PRESERVE", 2, true},
	{PRIMARY, "PRIMARY", 1, false},
	{PRIOR, "PRIOR", 2, true},
	{PRIVILEGES, "PRIVILEGES", 1, false},
	{PROCEDURE, "PROCEDURE", 1, false},
	{PROTECTED, "PROTECTED", 1, false},
	{RAND, "RAND", 2, false},
	{RANK, "RANK", 2, false},
	{DB_KEY, "RDB$DB_KEY", 1, false},
	{RDB_GET_CONTEXT, "RDB$GET_CONTEXT", 2, true},
	{RDB_SET_CONTEXT, "RDB$SET_CONTEXT", 2, true},
	{READ, "READ", 1, false},
	{REAL, "REAL", 1, false},
	{VERSION, "RECORD_VERSION", 1, false},
	{RECREATE, "RECREATE", 2, false},
	{RECURSIVE, "RECURSIVE", 2, false},
	{REFERENCES, "REFERENCES", 1, false},
	{KW_RELATIVE, "RELATIVE", 2, true},
	{RELEASE, "RELEASE", 2, false},
	{REPLACE, "REPLACE", 2, false},
	{REQUESTS, "REQUESTS", 2, true},
	{RESERVING, "RESERV", 1, false},	// Alias of RESERVING
	{RESERVING, "RESERVING", 1, false},
	{RESTART, "RESTART", 2, true},
	{RESTRICT, "RESTRICT", 1, true},
	{RETAIN, "RETAIN", 1, false},
	{RETURN, "RETURN", 2, false},
	{RETURNING, "RETURNING", 2, true},
	{RETURNING_VALUES, "RETURNING_VALUES", 1, false},
	{RETURNS, "RETURNS", 1, false},
	{REVERSE, "REVERSE", 2, false},
	{REVOKE, "REVOKE", 1, false},
	{RIGHT, "RIGHT", 1, false},
	{ROLE, "ROLE", 1, true},
	{ROLLBACK, "ROLLBACK", 1, false},
	{ROUND, "ROUND", 2, false},
	{ROW_COUNT, "ROW_COUNT", 2, false},
	{ROW_NUMBER, "ROW_NUMBER", 2, false},
	{ROWS, "ROWS", 2, false},
	{RPAD, "RPAD", 2, false},
	{SAVEPOINT, "SAVEPOINT", 2, false},
	{SCALAR_ARRAY, "SCALAR_ARRAY", 2, true},
	{DATABASE, "SCHEMA", 1, false},	// Alias of DATABASE
	{SCROLL, "SCROLL", 2, false},
	{SECOND, "SECOND", 2, false},
	{SEGMENT, "SEGMENT", 1, false},
	{SELECT, "SELECT", 1, false},
	{SENSITIVE, "SENSITIVE", 2, false},
	{SEQUENCE, "SEQUENCE", 2, true},
	{SET, "SET", 1, false},
	{SHADOW, "SHADOW", 1, false},
	{KW_SHARED, "SHARED", 1, false},
	{SIGN, "SIGN", 2, false},
	{SIMILAR, "SIMILAR", 2, false},
	{SIN, "SIN", 2, false},
	{SINGULAR, "SINGULAR", 1, false},
	{SINH, "SINH", 2, false},
	{KW_SIZE, "SIZE", 1, false},
	{SKIP, "SKIP", 2, true},
	{SMALLINT, "SMALLINT", 1, false},
	{SNAPSHOT, "SNAPSHOT", 1, false},
	{SOME, "SOME", 1, false},
	{SORT, "SORT", 1, false},
	{SOURCE, "SOURCE", 2, true},
	{SPACE, "SPACE", 2, true},
	{SQLCODE, "SQLCODE", 1, false},
	{SQLSTATE, "SQLSTATE", 2, false},
	{SQRT, "SQRT", 2, false},
	{STABILITY, "STABILITY", 1, false},
	{START, "START", 2, false},
	{STARTING, "STARTING", 1, false},
	{STARTING, "STARTS", 1, false},	// Alias of STARTING
	{STATEMENT, "STATEMENT", 2, true},
	{STATISTICS, "STATISTICS", 1, false},
	{SUBSTRING,	"SUBSTRING", 2, true},
	{SUB_TYPE, "SUB_TYPE", 1, false},
	{SUM, "SUM", 1, false},
	{SUSPEND, "SUSPEND", 1, false},
	{TABLE, "TABLE", 1, false},
	{TAN, "TAN", 2, false},
	{TANH, "TANH", 2, false},
	{TEMPORARY, "TEMPORARY", 2, true},
	{THEN, "THEN", 1, false},
	{TIME, "TIME", 2, false},
	{TIMESTAMP, "TIMESTAMP", 2, false},
	{TIMEOUT, "TIMEOUT", 2, true},
	{TO, "TO", 1, false},
	{TRAILING, "TRAILING", 2, false},
	{TRANSACTION, "TRANSACTION", 1, false},
	{TRIGGER, "TRIGGER", 1, false},
	{TRIM, "TRIM", 2, false},
	{KW_TRUE, "TRUE", 2, false},
	{TRUNC, "TRUNC", 2, false},
	{TWO_PHASE, "TWO_PHASE", 2, true},
	{KW_TYPE, "TYPE", 2, true},
	{UNCOMMITTED, "UNCOMMITTED", 1, false},
	{UNDO, "UNDO", 2, true},
	{UNION, "UNION", 1, false},
	{UNIQUE, "UNIQUE", 1, false},
	{UNKNOWN, "UNKNOWN", 2, false},
	{UPDATE, "UPDATE", 1, false},
	{UPDATING, "UPDATING", 2, true},
	{KW_UPPER, "UPPER", 1, false},
	{USER, "USER", 1, false},
	{USING, "USING", 2, false},
	{UUID_TO_CHAR, "UUID_TO_CHAR", 2, false},
	{KW_VALUE, "VALUE", 1, false},
	{VALUES, "VALUES", 1, false},
	{VARCHAR, "VARCHAR", 1, false},
	{VARIABLE, "VARIABLE", 1, false},
	{VARYING, "VARYING", 1, false},
	{VIEW, "VIEW", 1, false},
	{WAIT, "WAIT", 1, false},
	{WEEK, "WEEK", 2, false},
	{WEEKDAY, "WEEKDAY", 2, true},
	{WHEN, "WHEN", 1, false},
	{WHERE, "WHERE", 1, false},
	{WHILE, "WHILE", 1, false},
	{WITH, "WITH", 1, false},
	{WORK, "WORK", 1, false},
	{WRITE, "WRITE", 1, false},
	{YEAR, "YEAR", 2, false},
	{YEARDAY, "YEARDAY", 2, true},
	{NOT_LSS, "^<", 1, false},	// Alias of !<
	{NEQ, "^=", 1, false},				// Alias of !=
	{NOT_GTR, "^>", 1, false},			// Alias of !>
	{CONCATENATE, "||", 1, false},
	{NOT_LSS, "~<", 1, false},	// Alias of !<
	{NEQ, "~=", 1, false},				// Alias of !=
	{NOT_GTR, "~>", 1, false},			// Alias of !>
	{0, 0, 0, false}
};

// This method is currently used in isql/isql.epp to check if a
// user field is a reserved word, and hence needs to be quoted.
// Obviously a hash table would make this a little quicker.
// MOD 29-June-2002

extern "C" {

int API_ROUTINE KEYWORD_stringIsAToken(const char* in_str)
{
	const TOK* tok_ptr = tokens;
	while (tok_ptr->tok_string)
	{
		if (!tok_ptr->nonReserved && !strcmp(tok_ptr->tok_string, in_str)) {
			return true;
		}
		++tok_ptr;
	}
	return false;
}

Tokens API_ROUTINE KEYWORD_getTokens()
{
	return tokens;
}

}
