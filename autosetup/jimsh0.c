/* This is single source file, bootstrap version of Jim Tcl. See http://jim.tcl.tk/ */
#define JIM_TCL_COMPAT
#define JIM_ANSIC
#define JIM_REGEXP
#define HAVE_NO_AUTOCONF
#define _JIMAUTOCONF_H
#define TCL_LIBRARY "."
#define jim_ext_bootstrap
#define jim_ext_aio
#define jim_ext_readdir
#define jim_ext_regexp
#define jim_ext_file
#define jim_ext_glob
#define jim_ext_exec
#define jim_ext_clock
#define jim_ext_array
#define jim_ext_stdlib
#define jim_ext_tclcompat
#if defined(_MSC_VER)
#define TCL_PLATFORM_OS "windows"
#define TCL_PLATFORM_PLATFORM "windows"
#define TCL_PLATFORM_PATH_SEPARATOR ";"
#define HAVE_MKDIR_ONE_ARG
#define HAVE_SYSTEM
#elif defined(__MINGW32__)
#define TCL_PLATFORM_OS "mingw"
#define TCL_PLATFORM_PLATFORM "windows"
#define TCL_PLATFORM_PATH_SEPARATOR ";"
#define HAVE_MKDIR_ONE_ARG
#define HAVE_SYSTEM
#define HAVE_SYS_TIME_H
#define HAVE_DIRENT_H
#define HAVE_UNISTD_H
#define HAVE_UMASK
#include <sys/stat.h>
#ifndef S_IRWXG
#define S_IRWXG 0
#endif
#ifndef S_IRWXO
#define S_IRWXO 0
#endif
#else
#define TCL_PLATFORM_OS "unknown"
#define TCL_PLATFORM_PLATFORM "unix"
#define TCL_PLATFORM_PATH_SEPARATOR ":"
#ifdef _MINIX
#define vfork fork
#define _POSIX_SOURCE
#else
#define _GNU_SOURCE
#endif
#define HAVE_VFORK
#define HAVE_WAITPID
#define HAVE_ISATTY
#define HAVE_MKSTEMP
#define HAVE_LINK
#define HAVE_SYS_TIME_H
#define HAVE_DIRENT_H
#define HAVE_UNISTD_H
#define HAVE_UMASK
#endif
#define JIM_VERSION 78
#ifndef JIM_WIN32COMPAT_H
#define JIM_WIN32COMPAT_H



#ifdef __cplusplus
extern "C" {
#endif


#if defined(_WIN32) || defined(WIN32)

#define HAVE_DLOPEN
void *dlopen(const char *path, int mode);
int dlclose(void *handle);
void *dlsym(void *handle, const char *symbol);
char *dlerror(void);


#if defined(__MINGW32__)
    #define JIM_SPRINTF_DOUBLE_NEEDS_FIX
#endif

#ifdef _MSC_VER


#if _MSC_VER >= 1000
	#pragma warning(disable:4146)
#endif

#include <limits.h>
#define jim_wide _int64
#ifndef LLONG_MAX
	#define LLONG_MAX    9223372036854775807I64
#endif
#ifndef LLONG_MIN
	#define LLONG_MIN    (-LLONG_MAX - 1I64)
#endif
#define JIM_WIDE_MIN LLONG_MIN
#define JIM_WIDE_MAX LLONG_MAX
#define JIM_WIDE_MODIFIER "I64d"
#define strcasecmp _stricmp
#define strtoull _strtoui64

#include <io.h>

struct timeval {
	long tv_sec;
	long tv_usec;
};

int gettimeofday(struct timeval *tv, void *unused);

#define HAVE_OPENDIR
struct dirent {
	char *d_name;
};

typedef struct DIR {
	long                handle;
	struct _finddata_t  info;
	struct dirent       result;
	char                *name;
} DIR;

DIR *opendir(const char *name);
int closedir(DIR *dir);
struct dirent *readdir(DIR *dir);

#endif

#endif

#ifdef __cplusplus
}
#endif

#endif
#ifndef UTF8_UTIL_H
#define UTF8_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif



#define MAX_UTF8_LEN 4

int utf8_fromunicode(char *p, unsigned uc);

#ifndef JIM_UTF8
#include <ctype.h>


#define utf8_strlen(S, B) ((B) < 0 ? (int)strlen(S) : (B))
#define utf8_strwidth(S, B) utf8_strlen((S), (B))
#define utf8_tounicode(S, CP) (*(CP) = (unsigned char)*(S), 1)
#define utf8_getchars(CP, C) (*(CP) = (C), 1)
#define utf8_upper(C) toupper(C)
#define utf8_title(C) toupper(C)
#define utf8_lower(C) tolower(C)
#define utf8_index(C, I) (I)
#define utf8_charlen(C) 1
#define utf8_prev_len(S, L) 1
#define utf8_width(C) 1

#else

#endif

#ifdef __cplusplus
}
#endif

#endif

#ifndef __JIM__H
#define __JIM__H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>


#ifndef HAVE_NO_AUTOCONF
#endif



#ifndef jim_wide
#  ifdef HAVE_LONG_LONG
#    define jim_wide long long
#    ifndef LLONG_MAX
#      define LLONG_MAX    9223372036854775807LL
#    endif
#    ifndef LLONG_MIN
#      define LLONG_MIN    (-LLONG_MAX - 1LL)
#    endif
#    define JIM_WIDE_MIN LLONG_MIN
#    define JIM_WIDE_MAX LLONG_MAX
#  else
#    define jim_wide long
#    define JIM_WIDE_MIN LONG_MIN
#    define JIM_WIDE_MAX LONG_MAX
#  endif


#  ifdef HAVE_LONG_LONG
#    define JIM_WIDE_MODIFIER "lld"
#  else
#    define JIM_WIDE_MODIFIER "ld"
#    define strtoull strtoul
#  endif
#endif

#define UCHAR(c) ((unsigned char)(c))


#define JIM_OK 0
#define JIM_ERR 1
#define JIM_RETURN 2
#define JIM_BREAK 3
#define JIM_CONTINUE 4
#define JIM_SIGNAL 5
#define JIM_EXIT 6

#define JIM_EVAL 7

#define JIM_MAX_CALLFRAME_DEPTH 1000
#define JIM_MAX_EVAL_DEPTH 2000


#define JIM_PRIV_FLAG_SHIFT 20

#define JIM_NONE 0
#define JIM_ERRMSG 1
#define JIM_ENUM_ABBREV 2
#define JIM_UNSHARED 4
#define JIM_MUSTEXIST 8


#define JIM_SUBST_NOVAR 1
#define JIM_SUBST_NOCMD 2
#define JIM_SUBST_NOESC 4
#define JIM_SUBST_FLAG 128


#define JIM_CASESENS    0
#define JIM_NOCASE      1


#define JIM_PATH_LEN 1024


#define JIM_NOTUSED(V) ((void) V)

#define JIM_LIBPATH "auto_path"
#define JIM_INTERACTIVE "tcl_interactive"


typedef struct Jim_Stack {
    int len;
    int maxlen;
    void **vector;
} Jim_Stack;


typedef struct Jim_HashEntry {
    void *key;
    union {
        void *val;
        int intval;
    } u;
    struct Jim_HashEntry *next;
} Jim_HashEntry;

typedef struct Jim_HashTableType {
    unsigned int (*hashFunction)(const void *key);
    void *(*keyDup)(void *privdata, const void *key);
    void *(*valDup)(void *privdata, const void *obj);
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);
    void (*keyDestructor)(void *privdata, void *key);
    void (*valDestructor)(void *privdata, void *obj);
} Jim_HashTableType;

typedef struct Jim_HashTable {
    Jim_HashEntry **table;
    const Jim_HashTableType *type;
    void *privdata;
    unsigned int size;
    unsigned int sizemask;
    unsigned int used;
    unsigned int collisions;
    unsigned int uniq;
} Jim_HashTable;

typedef struct Jim_HashTableIterator {
    Jim_HashTable *ht;
    Jim_HashEntry *entry, *nextEntry;
    int index;
} Jim_HashTableIterator;


#define JIM_HT_INITIAL_SIZE     16


#define Jim_FreeEntryVal(ht, entry) \
    if ((ht)->type->valDestructor) \
        (ht)->type->valDestructor((ht)->privdata, (entry)->u.val)

#define Jim_SetHashVal(ht, entry, _val_) do { \
    if ((ht)->type->valDup) \
        (entry)->u.val = (ht)->type->valDup((ht)->privdata, (_val_)); \
    else \
        (entry)->u.val = (_val_); \
} while(0)

#define Jim_FreeEntryKey(ht, entry) \
    if ((ht)->type->keyDestructor) \
        (ht)->type->keyDestructor((ht)->privdata, (entry)->key)

#define Jim_SetHashKey(ht, entry, _key_) do { \
    if ((ht)->type->keyDup) \
        (entry)->key = (ht)->type->keyDup((ht)->privdata, (_key_)); \
    else \
        (entry)->key = (void *)(_key_); \
} while(0)

#define Jim_CompareHashKeys(ht, key1, key2) \
    (((ht)->type->keyCompare) ? \
        (ht)->type->keyCompare((ht)->privdata, (key1), (key2)) : \
        (key1) == (key2))

#define Jim_HashKey(ht, key) ((ht)->type->hashFunction(key) + (ht)->uniq)

#define Jim_GetHashEntryKey(he) ((he)->key)
#define Jim_GetHashEntryVal(he) ((he)->u.val)
#define Jim_GetHashTableCollisions(ht) ((ht)->collisions)
#define Jim_GetHashTableSize(ht) ((ht)->size)
#define Jim_GetHashTableUsed(ht) ((ht)->used)


typedef struct Jim_Obj {
    char *bytes;
    const struct Jim_ObjType *typePtr;
    int refCount;
    int length;

    union {

        jim_wide wideValue;

        int intValue;

        double doubleValue;

        void *ptr;

        struct {
            void *ptr1;
            void *ptr2;
        } twoPtrValue;

        struct {
            void *ptr;
            int int1;
            int int2;
        } ptrIntValue;

        struct {
            struct Jim_Var *varPtr;
            unsigned long callFrameId;
            int global;
        } varValue;

        struct {
            struct Jim_Obj *nsObj;
            struct Jim_Cmd *cmdPtr;
            unsigned long procEpoch;
        } cmdValue;

        struct {
            struct Jim_Obj **ele;
            int len;
            int maxLen;
        } listValue;

        struct {
            int maxLength;
            int charLength;
        } strValue;

        struct {
            unsigned long id;
            struct Jim_Reference *refPtr;
        } refValue;

        struct {
            struct Jim_Obj *fileNameObj;
            int lineNumber;
        } sourceValue;

        struct {
            struct Jim_Obj *varNameObjPtr;
            struct Jim_Obj *indexObjPtr;
        } dictSubstValue;
        struct {
            int line;
            int argc;
        } scriptLineValue;
    } internalRep;
    struct Jim_Obj *prevObjPtr;
    struct Jim_Obj *nextObjPtr;
} Jim_Obj;


#define Jim_IncrRefCount(objPtr) \
    ++(objPtr)->refCount
#define Jim_DecrRefCount(interp, objPtr) \
    if (--(objPtr)->refCount <= 0) Jim_FreeObj(interp, objPtr)
#define Jim_IsShared(objPtr) \
    ((objPtr)->refCount > 1)

#define Jim_FreeNewObj Jim_FreeObj


#define Jim_FreeIntRep(i,o) \
    if ((o)->typePtr && (o)->typePtr->freeIntRepProc) \
        (o)->typePtr->freeIntRepProc(i, o)


#define Jim_GetIntRepPtr(o) (o)->internalRep.ptr


#define Jim_SetIntRepPtr(o, p) \
    (o)->internalRep.ptr = (p)


struct Jim_Interp;

typedef void (Jim_FreeInternalRepProc)(struct Jim_Interp *interp,
        struct Jim_Obj *objPtr);
typedef void (Jim_DupInternalRepProc)(struct Jim_Interp *interp,
        struct Jim_Obj *srcPtr, Jim_Obj *dupPtr);
typedef void (Jim_UpdateStringProc)(struct Jim_Obj *objPtr);

typedef struct Jim_ObjType {
    const char *name;
    Jim_FreeInternalRepProc *freeIntRepProc;
    Jim_DupInternalRepProc *dupIntRepProc;
    Jim_UpdateStringProc *updateStringProc;
    int flags;
} Jim_ObjType;


#define JIM_TYPE_NONE 0
#define JIM_TYPE_REFERENCES 1



typedef struct Jim_CallFrame {
    unsigned long id;
    int level;
    struct Jim_HashTable vars;
    struct Jim_HashTable *staticVars;
    struct Jim_CallFrame *parent;
    Jim_Obj *const *argv;
    int argc;
    Jim_Obj *procArgsObjPtr;
    Jim_Obj *procBodyObjPtr;
    struct Jim_CallFrame *next;
    Jim_Obj *nsObj;
    Jim_Obj *fileNameObj;
    int line;
    Jim_Stack *localCommands;
    struct Jim_Obj *tailcallObj;
    struct Jim_Cmd *tailcallCmd;
} Jim_CallFrame;

typedef struct Jim_Var {
    Jim_Obj *objPtr;
    struct Jim_CallFrame *linkFramePtr;
} Jim_Var;


typedef int Jim_CmdProc(struct Jim_Interp *interp, int argc,
    Jim_Obj *const *argv);
typedef void Jim_DelCmdProc(struct Jim_Interp *interp, void *privData);



typedef struct Jim_Cmd {
    int inUse;
    int isproc;
    struct Jim_Cmd *prevCmd;
    union {
        struct {

            Jim_CmdProc *cmdProc;
            Jim_DelCmdProc *delProc;
            void *privData;
        } native;
        struct {

            Jim_Obj *argListObjPtr;
            Jim_Obj *bodyObjPtr;
            Jim_HashTable *staticVars;
            int argListLen;
            int reqArity;
            int optArity;
            int argsPos;
            int upcall;
            struct Jim_ProcArg {
                Jim_Obj *nameObjPtr;
                Jim_Obj *defaultObjPtr;
            } *arglist;
            Jim_Obj *nsObj;
        } proc;
    } u;
} Jim_Cmd;


typedef struct Jim_PrngState {
    unsigned char sbox[256];
    unsigned int i, j;
} Jim_PrngState;

typedef struct Jim_Interp {
    Jim_Obj *result;
    int errorLine;
    Jim_Obj *errorFileNameObj;
    int addStackTrace;
    int maxCallFrameDepth;
    int maxEvalDepth;
    int evalDepth;
    int returnCode;
    int returnLevel;
    int exitCode;
    long id;
    int signal_level;
    jim_wide sigmask;
    int (*signal_set_result)(struct Jim_Interp *interp, jim_wide sigmask);
    Jim_CallFrame *framePtr;
    Jim_CallFrame *topFramePtr;
    struct Jim_HashTable commands;
    unsigned long procEpoch; /* Incremented every time the result
                of procedures names lookup caching
                may no longer be valid. */
    unsigned long callFrameEpoch; /* Incremented every time a new
                callframe is created. This id is used for the
                'ID' field contained in the Jim_CallFrame
                structure. */
    int local;
    Jim_Obj *liveList;
    Jim_Obj *freeList;
    Jim_Obj *currentScriptObj;
    Jim_Obj *nullScriptObj;
    Jim_Obj *emptyObj;
    Jim_Obj *trueObj;
    Jim_Obj *falseObj;
    unsigned long referenceNextId;
    struct Jim_HashTable references;
    unsigned long lastCollectId; /* reference max Id of the last GC
                execution. It's set to ~0 while the collection
                is running as sentinel to avoid to recursive
                calls via the [collect] command inside
                finalizers. */
    time_t lastCollectTime;
    Jim_Obj *stackTrace;
    Jim_Obj *errorProc;
    Jim_Obj *unknown;
    int unknown_called;
    int errorFlag;
    void *cmdPrivData; /* Used to pass the private data pointer to
                  a command. It is set to what the user specified
                  via Jim_CreateCommand(). */

    struct Jim_CallFrame *freeFramesList;
    struct Jim_HashTable assocData;
    Jim_PrngState *prngState;
    struct Jim_HashTable packages;
    Jim_Stack *loadHandles;
} Jim_Interp;

#define Jim_InterpIncrProcEpoch(i) (i)->procEpoch++
#define Jim_SetResultString(i,s,l) Jim_SetResult(i, Jim_NewStringObj(i,s,l))
#define Jim_SetResultInt(i,intval) Jim_SetResult(i, Jim_NewIntObj(i,intval))

#define Jim_SetResultBool(i,b) Jim_SetResultInt(i, b)
#define Jim_SetEmptyResult(i) Jim_SetResult(i, (i)->emptyObj)
#define Jim_GetResult(i) ((i)->result)
#define Jim_CmdPrivData(i) ((i)->cmdPrivData)

#define Jim_SetResult(i,o) do {     \
    Jim_Obj *_resultObjPtr_ = (o);    \
    Jim_IncrRefCount(_resultObjPtr_); \
    Jim_DecrRefCount(i,(i)->result);  \
    (i)->result = _resultObjPtr_;     \
} while(0)


#define Jim_GetId(i) (++(i)->id)


#define JIM_REFERENCE_TAGLEN 7 /* The tag is fixed-length, because the reference
                                  string representation must be fixed length. */
typedef struct Jim_Reference {
    Jim_Obj *objPtr;
    Jim_Obj *finalizerCmdNamePtr;
    char tag[JIM_REFERENCE_TAGLEN+1];
} Jim_Reference;


#define Jim_NewEmptyStringObj(i) Jim_NewStringObj(i, "", 0)
#define Jim_FreeHashTableIterator(iter) Jim_Free(iter)

#define JIM_EXPORT


JIM_EXPORT void *Jim_Alloc (int size);
JIM_EXPORT void *Jim_Realloc(void *ptr, int size);
JIM_EXPORT void Jim_Free (void *ptr);
JIM_EXPORT char * Jim_StrDup (const char *s);
JIM_EXPORT char *Jim_StrDupLen(const char *s, int l);


JIM_EXPORT char **Jim_GetEnviron(void);
JIM_EXPORT void Jim_SetEnviron(char **env);
JIM_EXPORT int Jim_MakeTempFile(Jim_Interp *interp, const char *filename_template, int unlink_file);


JIM_EXPORT int Jim_Eval(Jim_Interp *interp, const char *script);


JIM_EXPORT int Jim_EvalSource(Jim_Interp *interp, const char *filename, int lineno, const char *script);

#define Jim_Eval_Named(I, S, F, L) Jim_EvalSource((I), (F), (L), (S))

JIM_EXPORT int Jim_EvalGlobal(Jim_Interp *interp, const char *script);
JIM_EXPORT int Jim_EvalFile(Jim_Interp *interp, const char *filename);
JIM_EXPORT int Jim_EvalFileGlobal(Jim_Interp *interp, const char *filename);
JIM_EXPORT int Jim_EvalObj (Jim_Interp *interp, Jim_Obj *scriptObjPtr);
JIM_EXPORT int Jim_EvalObjVector (Jim_Interp *interp, int objc,
        Jim_Obj *const *objv);
JIM_EXPORT int Jim_EvalObjList(Jim_Interp *interp, Jim_Obj *listObj);
JIM_EXPORT int Jim_EvalObjPrefix(Jim_Interp *interp, Jim_Obj *prefix,
        int objc, Jim_Obj *const *objv);
#define Jim_EvalPrefix(i, p, oc, ov) Jim_EvalObjPrefix((i), Jim_NewStringObj((i), (p), -1), (oc), (ov))
JIM_EXPORT int Jim_EvalNamespace(Jim_Interp *interp, Jim_Obj *scriptObj, Jim_Obj *nsObj);
JIM_EXPORT int Jim_SubstObj (Jim_Interp *interp, Jim_Obj *substObjPtr,
        Jim_Obj **resObjPtrPtr, int flags);


JIM_EXPORT void Jim_InitStack(Jim_Stack *stack);
JIM_EXPORT void Jim_FreeStack(Jim_Stack *stack);
JIM_EXPORT int Jim_StackLen(Jim_Stack *stack);
JIM_EXPORT void Jim_StackPush(Jim_Stack *stack, void *element);
JIM_EXPORT void * Jim_StackPop(Jim_Stack *stack);
JIM_EXPORT void * Jim_StackPeek(Jim_Stack *stack);
JIM_EXPORT void Jim_FreeStackElements(Jim_Stack *stack, void (*freeFunc)(void *ptr));


JIM_EXPORT int Jim_InitHashTable (Jim_HashTable *ht,
        const Jim_HashTableType *type, void *privdata);
JIM_EXPORT void Jim_ExpandHashTable (Jim_HashTable *ht,
        unsigned int size);
JIM_EXPORT int Jim_AddHashEntry (Jim_HashTable *ht, const void *key,
        void *val);
JIM_EXPORT int Jim_ReplaceHashEntry (Jim_HashTable *ht,
        const void *key, void *val);
JIM_EXPORT int Jim_DeleteHashEntry (Jim_HashTable *ht,
        const void *key);
JIM_EXPORT int Jim_FreeHashTable (Jim_HashTable *ht);
JIM_EXPORT Jim_HashEntry * Jim_FindHashEntry (Jim_HashTable *ht,
        const void *key);
JIM_EXPORT void Jim_ResizeHashTable (Jim_HashTable *ht);
JIM_EXPORT Jim_HashTableIterator *Jim_GetHashTableIterator
        (Jim_HashTable *ht);
JIM_EXPORT Jim_HashEntry * Jim_NextHashEntry
        (Jim_HashTableIterator *iter);


JIM_EXPORT Jim_Obj * Jim_NewObj (Jim_Interp *interp);
JIM_EXPORT void Jim_FreeObj (Jim_Interp *interp, Jim_Obj *objPtr);
JIM_EXPORT void Jim_InvalidateStringRep (Jim_Obj *objPtr);
JIM_EXPORT Jim_Obj * Jim_DuplicateObj (Jim_Interp *interp,
        Jim_Obj *objPtr);
JIM_EXPORT const char * Jim_GetString(Jim_Obj *objPtr,
        int *lenPtr);
JIM_EXPORT const char *Jim_String(Jim_Obj *objPtr);
JIM_EXPORT int Jim_Length(Jim_Obj *objPtr);


JIM_EXPORT Jim_Obj * Jim_NewStringObj (Jim_Interp *interp,
        const char *s, int len);
JIM_EXPORT Jim_Obj *Jim_NewStringObjUtf8(Jim_Interp *interp,
        const char *s, int charlen);
JIM_EXPORT Jim_Obj * Jim_NewStringObjNoAlloc (Jim_Interp *interp,
        char *s, int len);
JIM_EXPORT void Jim_AppendString (Jim_Interp *interp, Jim_Obj *objPtr,
        const char *str, int len);
JIM_EXPORT void Jim_AppendObj (Jim_Interp *interp, Jim_Obj *objPtr,
        Jim_Obj *appendObjPtr);
JIM_EXPORT void Jim_AppendStrings (Jim_Interp *interp,
        Jim_Obj *objPtr, ...);
JIM_EXPORT int Jim_StringEqObj(Jim_Obj *aObjPtr, Jim_Obj *bObjPtr);
JIM_EXPORT int Jim_StringMatchObj (Jim_Interp *interp, Jim_Obj *patternObjPtr,
        Jim_Obj *objPtr, int nocase);
JIM_EXPORT Jim_Obj * Jim_StringRangeObj (Jim_Interp *interp,
        Jim_Obj *strObjPtr, Jim_Obj *firstObjPtr,
        Jim_Obj *lastObjPtr);
JIM_EXPORT Jim_Obj * Jim_FormatString (Jim_Interp *interp,
        Jim_Obj *fmtObjPtr, int objc, Jim_Obj *const *objv);
JIM_EXPORT Jim_Obj * Jim_ScanString (Jim_Interp *interp, Jim_Obj *strObjPtr,
        Jim_Obj *fmtObjPtr, int flags);
JIM_EXPORT int Jim_CompareStringImmediate (Jim_Interp *interp,
        Jim_Obj *objPtr, const char *str);
JIM_EXPORT int Jim_StringCompareObj(Jim_Interp *interp, Jim_Obj *firstObjPtr,
        Jim_Obj *secondObjPtr, int nocase);
JIM_EXPORT int Jim_StringCompareLenObj(Jim_Interp *interp, Jim_Obj *firstObjPtr,
        Jim_Obj *secondObjPtr, int nocase);
JIM_EXPORT int Jim_Utf8Length(Jim_Interp *interp, Jim_Obj *objPtr);


JIM_EXPORT Jim_Obj * Jim_NewReference (Jim_Interp *interp,
        Jim_Obj *objPtr, Jim_Obj *tagPtr, Jim_Obj *cmdNamePtr);
JIM_EXPORT Jim_Reference * Jim_GetReference (Jim_Interp *interp,
        Jim_Obj *objPtr);
JIM_EXPORT int Jim_SetFinalizer (Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *cmdNamePtr);
JIM_EXPORT int Jim_GetFinalizer (Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj **cmdNamePtrPtr);


JIM_EXPORT Jim_Interp * Jim_CreateInterp (void);
JIM_EXPORT void Jim_FreeInterp (Jim_Interp *i);
JIM_EXPORT int Jim_GetExitCode (Jim_Interp *interp);
JIM_EXPORT const char *Jim_ReturnCode(int code);
JIM_EXPORT void Jim_SetResultFormatted(Jim_Interp *interp, const char *format, ...);


JIM_EXPORT void Jim_RegisterCoreCommands (Jim_Interp *interp);
JIM_EXPORT int Jim_CreateCommand (Jim_Interp *interp,
        const char *cmdName, Jim_CmdProc *cmdProc, void *privData,
         Jim_DelCmdProc *delProc);
JIM_EXPORT int Jim_DeleteCommand (Jim_Interp *interp,
        const char *cmdName);
JIM_EXPORT int Jim_RenameCommand (Jim_Interp *interp,
        const char *oldName, const char *newName);
JIM_EXPORT Jim_Cmd * Jim_GetCommand (Jim_Interp *interp,
        Jim_Obj *objPtr, int flags);
JIM_EXPORT int Jim_SetVariable (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, Jim_Obj *valObjPtr);
JIM_EXPORT int Jim_SetVariableStr (Jim_Interp *interp,
        const char *name, Jim_Obj *objPtr);
JIM_EXPORT int Jim_SetGlobalVariableStr (Jim_Interp *interp,
        const char *name, Jim_Obj *objPtr);
JIM_EXPORT int Jim_SetVariableStrWithStr (Jim_Interp *interp,
        const char *name, const char *val);
JIM_EXPORT int Jim_SetVariableLink (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, Jim_Obj *targetNameObjPtr,
        Jim_CallFrame *targetCallFrame);
JIM_EXPORT Jim_Obj * Jim_MakeGlobalNamespaceName(Jim_Interp *interp,
        Jim_Obj *nameObjPtr);
JIM_EXPORT Jim_Obj * Jim_GetVariable (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, int flags);
JIM_EXPORT Jim_Obj * Jim_GetGlobalVariable (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, int flags);
JIM_EXPORT Jim_Obj * Jim_GetVariableStr (Jim_Interp *interp,
        const char *name, int flags);
JIM_EXPORT Jim_Obj * Jim_GetGlobalVariableStr (Jim_Interp *interp,
        const char *name, int flags);
JIM_EXPORT int Jim_UnsetVariable (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, int flags);


JIM_EXPORT Jim_CallFrame *Jim_GetCallFrameByLevel(Jim_Interp *interp,
        Jim_Obj *levelObjPtr);


JIM_EXPORT int Jim_Collect (Jim_Interp *interp);
JIM_EXPORT void Jim_CollectIfNeeded (Jim_Interp *interp);


JIM_EXPORT int Jim_GetIndex (Jim_Interp *interp, Jim_Obj *objPtr,
        int *indexPtr);


JIM_EXPORT Jim_Obj * Jim_NewListObj (Jim_Interp *interp,
        Jim_Obj *const *elements, int len);
JIM_EXPORT void Jim_ListInsertElements (Jim_Interp *interp,
        Jim_Obj *listPtr, int listindex, int objc, Jim_Obj *const *objVec);
JIM_EXPORT void Jim_ListAppendElement (Jim_Interp *interp,
        Jim_Obj *listPtr, Jim_Obj *objPtr);
JIM_EXPORT void Jim_ListAppendList (Jim_Interp *interp,
        Jim_Obj *listPtr, Jim_Obj *appendListPtr);
JIM_EXPORT int Jim_ListLength (Jim_Interp *interp, Jim_Obj *objPtr);
JIM_EXPORT int Jim_ListIndex (Jim_Interp *interp, Jim_Obj *listPrt,
        int listindex, Jim_Obj **objPtrPtr, int seterr);
JIM_EXPORT Jim_Obj *Jim_ListGetIndex(Jim_Interp *interp, Jim_Obj *listPtr, int idx);
JIM_EXPORT int Jim_SetListIndex (Jim_Interp *interp,
        Jim_Obj *varNamePtr, Jim_Obj *const *indexv, int indexc,
        Jim_Obj *newObjPtr);
JIM_EXPORT Jim_Obj * Jim_ConcatObj (Jim_Interp *interp, int objc,
        Jim_Obj *const *objv);
JIM_EXPORT Jim_Obj *Jim_ListJoin(Jim_Interp *interp,
        Jim_Obj *listObjPtr, const char *joinStr, int joinStrLen);


JIM_EXPORT Jim_Obj * Jim_NewDictObj (Jim_Interp *interp,
        Jim_Obj *const *elements, int len);
JIM_EXPORT int Jim_DictKey (Jim_Interp *interp, Jim_Obj *dictPtr,
        Jim_Obj *keyPtr, Jim_Obj **objPtrPtr, int flags);
JIM_EXPORT int Jim_DictKeysVector (Jim_Interp *interp,
        Jim_Obj *dictPtr, Jim_Obj *const *keyv, int keyc,
        Jim_Obj **objPtrPtr, int flags);
JIM_EXPORT int Jim_SetDictKeysVector (Jim_Interp *interp,
        Jim_Obj *varNamePtr, Jim_Obj *const *keyv, int keyc,
        Jim_Obj *newObjPtr, int flags);
JIM_EXPORT int Jim_DictPairs(Jim_Interp *interp,
        Jim_Obj *dictPtr, Jim_Obj ***objPtrPtr, int *len);
JIM_EXPORT int Jim_DictAddElement(Jim_Interp *interp, Jim_Obj *objPtr,
        Jim_Obj *keyObjPtr, Jim_Obj *valueObjPtr);

#define JIM_DICTMATCH_KEYS 0x0001
#define JIM_DICTMATCH_VALUES 0x002

JIM_EXPORT int Jim_DictMatchTypes(Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *patternObj, int match_type, int return_types);
JIM_EXPORT int Jim_DictSize(Jim_Interp *interp, Jim_Obj *objPtr);
JIM_EXPORT int Jim_DictInfo(Jim_Interp *interp, Jim_Obj *objPtr);
JIM_EXPORT Jim_Obj *Jim_DictMerge(Jim_Interp *interp, int objc, Jim_Obj *const *objv);


JIM_EXPORT int Jim_GetReturnCode (Jim_Interp *interp, Jim_Obj *objPtr,
        int *intPtr);


JIM_EXPORT int Jim_EvalExpression (Jim_Interp *interp,
        Jim_Obj *exprObjPtr);
JIM_EXPORT int Jim_GetBoolFromExpr (Jim_Interp *interp,
        Jim_Obj *exprObjPtr, int *boolPtr);


JIM_EXPORT int Jim_GetBoolean(Jim_Interp *interp, Jim_Obj *objPtr,
        int *booleanPtr);


JIM_EXPORT int Jim_GetWide (Jim_Interp *interp, Jim_Obj *objPtr,
        jim_wide *widePtr);
JIM_EXPORT int Jim_GetLong (Jim_Interp *interp, Jim_Obj *objPtr,
        long *longPtr);
#define Jim_NewWideObj  Jim_NewIntObj
JIM_EXPORT Jim_Obj * Jim_NewIntObj (Jim_Interp *interp,
        jim_wide wideValue);


JIM_EXPORT int Jim_GetDouble(Jim_Interp *interp, Jim_Obj *objPtr,
        double *doublePtr);
JIM_EXPORT void Jim_SetDouble(Jim_Interp *interp, Jim_Obj *objPtr,
        double doubleValue);
JIM_EXPORT Jim_Obj * Jim_NewDoubleObj(Jim_Interp *interp, double doubleValue);


JIM_EXPORT void Jim_WrongNumArgs (Jim_Interp *interp, int argc,
        Jim_Obj *const *argv, const char *msg);
JIM_EXPORT int Jim_GetEnum (Jim_Interp *interp, Jim_Obj *objPtr,
        const char * const *tablePtr, int *indexPtr, const char *name, int flags);
JIM_EXPORT int Jim_CheckShowCommands(Jim_Interp *interp, Jim_Obj *objPtr,
        const char *const *tablePtr);
JIM_EXPORT int Jim_ScriptIsComplete(Jim_Interp *interp,
        Jim_Obj *scriptObj, char *stateCharPtr);

JIM_EXPORT int Jim_FindByName(const char *name, const char * const array[], size_t len);


typedef void (Jim_InterpDeleteProc)(Jim_Interp *interp, void *data);
JIM_EXPORT void * Jim_GetAssocData(Jim_Interp *interp, const char *key);
JIM_EXPORT int Jim_SetAssocData(Jim_Interp *interp, const char *key,
        Jim_InterpDeleteProc *delProc, void *data);
JIM_EXPORT int Jim_DeleteAssocData(Jim_Interp *interp, const char *key);



JIM_EXPORT int Jim_PackageProvide (Jim_Interp *interp,
        const char *name, const char *ver, int flags);
JIM_EXPORT int Jim_PackageRequire (Jim_Interp *interp,
        const char *name, int flags);


JIM_EXPORT void Jim_MakeErrorMessage (Jim_Interp *interp);


JIM_EXPORT int Jim_InteractivePrompt (Jim_Interp *interp);
JIM_EXPORT void Jim_HistoryLoad(const char *filename);
JIM_EXPORT void Jim_HistorySave(const char *filename);
JIM_EXPORT char *Jim_HistoryGetline(Jim_Interp *interp, const char *prompt);
JIM_EXPORT void Jim_HistorySetCompletion(Jim_Interp *interp, Jim_Obj *commandObj);
JIM_EXPORT void Jim_HistoryAdd(const char *line);
JIM_EXPORT void Jim_HistoryShow(void);


JIM_EXPORT int Jim_InitStaticExtensions(Jim_Interp *interp);
JIM_EXPORT int Jim_StringToWide(const char *str, jim_wide *widePtr, int base);
JIM_EXPORT int Jim_IsBigEndian(void);

#define Jim_CheckSignal(i) ((i)->signal_level && (i)->sigmask)


JIM_EXPORT int Jim_LoadLibrary(Jim_Interp *interp, const char *pathName);
JIM_EXPORT void Jim_FreeLoadHandles(Jim_Interp *interp);


JIM_EXPORT FILE *Jim_AioFilehandle(Jim_Interp *interp, Jim_Obj *command);


JIM_EXPORT int Jim_IsDict(Jim_Obj *objPtr);
JIM_EXPORT int Jim_IsList(Jim_Obj *objPtr);

#ifdef __cplusplus
}
#endif

#endif

#ifndef JIM_SUBCMD_H
#define JIM_SUBCMD_H


#ifdef __cplusplus
extern "C" {
#endif


#define JIM_MODFLAG_HIDDEN   0x0001
#define JIM_MODFLAG_FULLARGV 0x0002



typedef int jim_subcmd_function(Jim_Interp *interp, int argc, Jim_Obj *const *argv);

typedef struct {
	const char *cmd;
	const char *args;
	jim_subcmd_function *function;
	short minargs;
	short maxargs;
	unsigned short flags;
} jim_subcmd_type;

const jim_subcmd_type *
Jim_ParseSubCmd(Jim_Interp *interp, const jim_subcmd_type *command_table, int argc, Jim_Obj *const *argv);

int Jim_SubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv);

int Jim_CallSubCmd(Jim_Interp *interp, const jim_subcmd_type *ct, int argc, Jim_Obj *const *argv);

#ifdef __cplusplus
}
#endif

#endif
#ifndef JIMREGEXP_H
#define JIMREGEXP_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

typedef struct {
	int rm_so;
	int rm_eo;
} regmatch_t;


typedef struct regexp {

	int re_nsub;


	int cflags;
	int err;
	int regstart;
	int reganch;
	int regmust;
	int regmlen;
	int *program;


	const char *regparse;
	int p;
	int proglen;


	int eflags;
	const char *start;
	const char *reginput;
	const char *regbol;


	regmatch_t *pmatch;
	int nmatch;
} regexp;

typedef regexp regex_t;

#define REG_EXTENDED 0
#define REG_NEWLINE 1
#define REG_ICASE 2

#define REG_NOTBOL 16

enum {
	REG_NOERROR,
	REG_NOMATCH,
	REG_BADPAT,
	REG_ERR_NULL_ARGUMENT,
	REG_ERR_UNKNOWN,
	REG_ERR_TOO_BIG,
	REG_ERR_NOMEM,
	REG_ERR_TOO_MANY_PAREN,
	REG_ERR_UNMATCHED_PAREN,
	REG_ERR_UNMATCHED_BRACES,
	REG_ERR_BAD_COUNT,
	REG_ERR_JUNK_ON_END,
	REG_ERR_OPERAND_COULD_BE_EMPTY,
	REG_ERR_NESTED_COUNT,
	REG_ERR_INTERNAL,
	REG_ERR_COUNT_FOLLOWS_NOTHING,
	REG_ERR_TRAILING_BACKSLASH,
	REG_ERR_CORRUPTED,
	REG_ERR_NULL_CHAR,
	REG_ERR_NUM
};

int regcomp(regex_t *preg, const char *regex, int cflags);
int regexec(regex_t  *preg,  const  char *string, size_t nmatch, regmatch_t pmatch[], int eflags);
size_t regerror(int errcode, const regex_t *preg, char *errbuf,  size_t errbuf_size);
void regfree(regex_t *preg);

#ifdef __cplusplus
}
#endif

#endif
#ifndef JIM_SIGNAL_H
#define JIM_SIGNAL_H

#ifdef __cplusplus
extern "C" {
#endif

const char *Jim_SignalId(int sig);

#ifdef __cplusplus
}
#endif

#endif
#ifndef JIMIOCOMPAT_H
#define JIMIOCOMPAT_H


#include <stdio.h>
#include <errno.h>


void Jim_SetResultErrno(Jim_Interp *interp, const char *msg);

int Jim_OpenForWrite(const char *filename, int append);

int Jim_OpenForRead(const char *filename);

#if defined(__MINGW32__)
    #ifndef STRICT
    #define STRICT
    #endif
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <fcntl.h>
    #include <io.h>
    #include <process.h>

    typedef HANDLE pidtype;
    #define JIM_BAD_PID INVALID_HANDLE_VALUE

    #define JIM_NO_PID INVALID_HANDLE_VALUE


    #define WIFEXITED(STATUS) (((STATUS) & 0xff00) == 0)
    #define WEXITSTATUS(STATUS) ((STATUS) & 0x00ff)
    #define WIFSIGNALED(STATUS) (((STATUS) & 0xff00) != 0)
    #define WTERMSIG(STATUS) (((STATUS) >> 8) & 0xff)
    #define WNOHANG 1

    int Jim_Errno(void);
    pidtype waitpid(pidtype pid, int *status, int nohang);

    #define HAVE_PIPE
    #define pipe(P) _pipe((P), 0, O_NOINHERIT)

#elif defined(HAVE_UNISTD_H)
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/wait.h>
    #include <sys/stat.h>

    typedef int pidtype;
    #define Jim_Errno() errno
    #define JIM_BAD_PID -1
    #define JIM_NO_PID 0

    #ifndef HAVE_EXECVPE
        #define execvpe(ARG0, ARGV, ENV) execvp(ARG0, ARGV)
    #endif
#endif

#endif
int Jim_bootstrapInit(Jim_Interp *interp)
{
	if (Jim_PackageProvide(interp, "bootstrap", "1.0", JIM_ERRMSG))
		return JIM_ERR;

	return Jim_EvalSource(interp, "bootstrap.tcl", 1,
"\n"
"\n"
"proc package {cmd pkg args} {\n"
"	if {$cmd eq \"require\"} {\n"
"		foreach path $::auto_path {\n"
"			set pkgpath $path/$pkg.tcl\n"
"			if {$path eq \".\"} {\n"
"				set pkgpath $pkg.tcl\n"
"			}\n"
"			if {[file exists $pkgpath]} {\n"
"				uplevel #0 [list source $pkgpath]\n"
"				return\n"
"			}\n"
"		}\n"
"	}\n"
"}\n"
);
}
int Jim_initjimshInit(Jim_Interp *interp)
{
	if (Jim_PackageProvide(interp, "initjimsh", "1.0", JIM_ERRMSG))
		return JIM_ERR;

	return Jim_EvalSource(interp, "initjimsh.tcl", 1,
"\n"
"\n"
"\n"
"proc _jimsh_init {} {\n"
"	rename _jimsh_init {}\n"
"	global jim::exe jim::argv0 tcl_interactive auto_path tcl_platform\n"
"\n"
"\n"
"	if {[exists jim::argv0]} {\n"
"		if {[string match \"*/*\" $jim::argv0]} {\n"
"			set jim::exe [file join [pwd] $jim::argv0]\n"
"		} else {\n"
"			foreach path [split [env PATH \"\"] $tcl_platform(pathSeparator)] {\n"
"				set exec [file join [pwd] [string map {\\\\ /} $path] $jim::argv0]\n"
"				if {[file executable $exec]} {\n"
"					set jim::exe $exec\n"
"					break\n"
"				}\n"
"			}\n"
"		}\n"
"	}\n"
"\n"
"\n"
"	lappend p {*}[split [env JIMLIB {}] $tcl_platform(pathSeparator)]\n"
"	if {[exists jim::exe]} {\n"
"		lappend p [file dirname $jim::exe]\n"
"	}\n"
"	lappend p {*}$auto_path\n"
"	set auto_path $p\n"
"\n"
"	if {$tcl_interactive && [env HOME {}] ne \"\"} {\n"
"		foreach src {.jimrc jimrc.tcl} {\n"
"			if {[file exists [env HOME]/$src]} {\n"
"				uplevel #0 source [env HOME]/$src\n"
"				break\n"
"			}\n"
"		}\n"
"	}\n"
"	return \"\"\n"
"}\n"
"\n"
"if {$tcl_platform(platform) eq \"windows\"} {\n"
"	set jim::argv0 [string map {\\\\ /} $jim::argv0]\n"
"}\n"
"\n"
"\n"
"set tcl::autocomplete_commands {info tcl::prefix socket namespace array clock file package string dict signal history}\n"
"\n"
"\n"
"\n"
"proc tcl::autocomplete {prefix} {\n"
"	if {[set space [string first \" \" $prefix]] != -1} {\n"
"		set cmd [string range $prefix 0 $space-1]\n"
"		if {$cmd in $::tcl::autocomplete_commands || [info channel $cmd] ne \"\"} {\n"
"			set arg [string range $prefix $space+1 end]\n"
"\n"
"			return [lmap p [$cmd -commands] {\n"
"				if {![string match \"${arg}*\" $p]} continue\n"
"				function \"$cmd $p\"\n"
"			}]\n"
"		}\n"
"	}\n"
"\n"
"	if {[string match \"source *\" $prefix]} {\n"
"		set path [string range $prefix 7 end]\n"
"		return [lmap p [glob -nocomplain \"${path}*\"] {\n"
"			function \"source $p\"\n"
"		}]\n"
"	}\n"
"\n"
"	return [lmap p [lsort [info commands $prefix*]] {\n"
"		if {[string match \"* *\" $p]} {\n"
"			continue\n"
"		}\n"
"		function $p\n"
"	}]\n"
"}\n"
"\n"
"_jimsh_init\n"
);
}
int Jim_globInit(Jim_Interp *interp)
{
	if (Jim_PackageProvide(interp, "glob", "1.0", JIM_ERRMSG))
		return JIM_ERR;

	return Jim_EvalSource(interp, "glob.tcl", 1,
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"package require readdir\n"
"\n"
"\n"
"proc glob.globdir {dir pattern} {\n"
"	if {[file exists $dir/$pattern]} {\n"
"\n"
"		return [list $pattern]\n"
"	}\n"
"\n"
"	set result {}\n"
"	set files [readdir $dir]\n"
"	lappend files . ..\n"
"\n"
"	foreach name $files {\n"
"		if {[string match $pattern $name]} {\n"
"\n"
"			if {[string index $name 0] eq \".\" && [string index $pattern 0] ne \".\"} {\n"
"				continue\n"
"			}\n"
"			lappend result $name\n"
"		}\n"
"	}\n"
"\n"
"	return $result\n"
"}\n"
"\n"
"\n"
"\n"
"\n"
"proc glob.explode {pattern} {\n"
"	set oldexp {}\n"
"	set newexp {\"\"}\n"
"\n"
"	while 1 {\n"
"		set oldexp $newexp\n"
"		set newexp {}\n"
"		set ob [string first \\{ $pattern]\n"
"		set cb [string first \\} $pattern]\n"
"\n"
"		if {$ob < $cb && $ob != -1} {\n"
"			set mid [string range $pattern 0 $ob-1]\n"
"			set subexp [lassign [glob.explode [string range $pattern $ob+1 end]] pattern]\n"
"			if {$pattern eq \"\"} {\n"
"				error \"unmatched open brace in glob pattern\"\n"
"			}\n"
"			set pattern [string range $pattern 1 end]\n"
"\n"
"			foreach subs $subexp {\n"
"				foreach sub [split $subs ,] {\n"
"					foreach old $oldexp {\n"
"						lappend newexp $old$mid$sub\n"
"					}\n"
"				}\n"
"			}\n"
"		} elseif {$cb != -1} {\n"
"			set suf  [string range $pattern 0 $cb-1]\n"
"			set rest [string range $pattern $cb end]\n"
"			break\n"
"		} else {\n"
"			set suf  $pattern\n"
"			set rest \"\"\n"
"			break\n"
"		}\n"
"	}\n"
"\n"
"	foreach old $oldexp {\n"
"		lappend newexp $old$suf\n"
"	}\n"
"	list $rest {*}$newexp\n"
"}\n"
"\n"
"\n"
"\n"
"proc glob.glob {base pattern} {\n"
"	set dir [file dirname $pattern]\n"
"	if {$pattern eq $dir || $pattern eq \"\"} {\n"
"		return [list [file join $base $dir] $pattern]\n"
"	} elseif {$pattern eq [file tail $pattern]} {\n"
"		set dir \"\"\n"
"	}\n"
"\n"
"\n"
"	set dirlist [glob.glob $base $dir]\n"
"	set pattern [file tail $pattern]\n"
"\n"
"\n"
"	set result {}\n"
"	foreach {realdir dir} $dirlist {\n"
"		if {![file isdir $realdir]} {\n"
"			continue\n"
"		}\n"
"		if {[string index $dir end] ne \"/\" && $dir ne \"\"} {\n"
"			append dir /\n"
"		}\n"
"		foreach name [glob.globdir $realdir $pattern] {\n"
"			lappend result [file join $realdir $name] $dir$name\n"
"		}\n"
"	}\n"
"	return $result\n"
"}\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"proc glob {args} {\n"
"	set nocomplain 0\n"
"	set base \"\"\n"
"	set tails 0\n"
"\n"
"	set n 0\n"
"	foreach arg $args {\n"
"		if {[info exists param]} {\n"
"			set $param $arg\n"
"			unset param\n"
"			incr n\n"
"			continue\n"
"		}\n"
"		switch -glob -- $arg {\n"
"			-d* {\n"
"				set switch $arg\n"
"				set param base\n"
"			}\n"
"			-n* {\n"
"				set nocomplain 1\n"
"			}\n"
"			-ta* {\n"
"				set tails 1\n"
"			}\n"
"			-- {\n"
"				incr n\n"
"				break\n"
"			}\n"
"			-* {\n"
"				return -code error \"bad option \\\"$arg\\\": must be -directory, -nocomplain, -tails, or --\"\n"
"			}\n"
"			*  {\n"
"				break\n"
"			}\n"
"		}\n"
"		incr n\n"
"	}\n"
"	if {[info exists param]} {\n"
"		return -code error \"missing argument to \\\"$switch\\\"\"\n"
"	}\n"
"	if {[llength $args] <= $n} {\n"
"		return -code error \"wrong # args: should be \\\"glob ?options? pattern ?pattern ...?\\\"\"\n"
"	}\n"
"\n"
"	set args [lrange $args $n end]\n"
"\n"
"	set result {}\n"
"	foreach pattern $args {\n"
"		set escpattern [string map {\n"
"			\\\\\\\\ \\x01 \\\\\\{ \\x02 \\\\\\} \\x03 \\\\, \\x04\n"
"		} $pattern]\n"
"		set patexps [lassign [glob.explode $escpattern] rest]\n"
"		if {$rest ne \"\"} {\n"
"			return -code error \"unmatched close brace in glob pattern\"\n"
"		}\n"
"		foreach patexp $patexps {\n"
"			set patexp [string map {\n"
"				\\x01 \\\\\\\\ \\x02 \\{ \\x03 \\} \\x04 ,\n"
"			} $patexp]\n"
"			foreach {realname name} [glob.glob $base $patexp] {\n"
"				incr n\n"
"				if {$tails} {\n"
"					lappend result $name\n"
"				} else {\n"
"					lappend result [file join $base $name]\n"
"				}\n"
"			}\n"
"		}\n"
"	}\n"
"\n"
"	if {!$nocomplain && [llength $result] == 0} {\n"
"		set s $(([llength $args] > 1) ? \"s\" : \"\")\n"
"		return -code error \"no files matched glob pattern$s \\\"[join $args]\\\"\"\n"
"	}\n"
"\n"
"	return $result\n"
"}\n"
);
}
int Jim_stdlibInit(Jim_Interp *interp)
{
	if (Jim_PackageProvide(interp, "stdlib", "1.0", JIM_ERRMSG))
		return JIM_ERR;

	return Jim_EvalSource(interp, "stdlib.tcl", 1,
"\n"
"\n"
"if {![exists -command ref]} {\n"
"\n"
"	proc ref {args} {{count 0}} {\n"
"		format %08x [incr count]\n"
"	}\n"
"}\n"
"\n"
"\n"
"proc lambda {arglist args} {\n"
"	tailcall proc [ref {} function lambda.finalizer] $arglist {*}$args\n"
"}\n"
"\n"
"proc lambda.finalizer {name val} {\n"
"	rename $name {}\n"
"}\n"
"\n"
"\n"
"proc curry {args} {\n"
"	alias [ref {} function lambda.finalizer] {*}$args\n"
"}\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"proc function {value} {\n"
"	return $value\n"
"}\n"
"\n"
"\n"
"\n"
"\n"
"proc stacktrace {{skip 0}} {\n"
"	set trace {}\n"
"	incr skip\n"
"	foreach level [range $skip [info level]] {\n"
"		lappend trace {*}[info frame -$level]\n"
"	}\n"
"	return $trace\n"
"}\n"
"\n"
"\n"
"proc stackdump {stacktrace} {\n"
"	set lines {}\n"
"	foreach {l f p} [lreverse $stacktrace] {\n"
"		set line {}\n"
"		if {$p ne \"\"} {\n"
"			append line \"in procedure '$p' \"\n"
"			if {$f ne \"\"} {\n"
"				append line \"called \"\n"
"			}\n"
"		}\n"
"		if {$f ne \"\"} {\n"
"			append line \"at file \\\"$f\\\", line $l\"\n"
"		}\n"
"		if {$line ne \"\"} {\n"
"			lappend lines $line\n"
"		}\n"
"	}\n"
"	join $lines \\n\n"
"}\n"
"\n"
"\n"
"\n"
"proc defer {script} {\n"
"	upvar jim::defer v\n"
"	lappend v $script\n"
"}\n"
"\n"
"\n"
"\n"
"proc errorInfo {msg {stacktrace \"\"}} {\n"
"	if {$stacktrace eq \"\"} {\n"
"\n"
"		set stacktrace [info stacktrace]\n"
"\n"
"		lappend stacktrace {*}[stacktrace 1]\n"
"	}\n"
"	lassign $stacktrace p f l\n"
"	if {$f ne \"\"} {\n"
"		set result \"$f:$l: Error: \"\n"
"	}\n"
"	append result \"$msg\\n\"\n"
"	append result [stackdump $stacktrace]\n"
"\n"
"\n"
"	string trim $result\n"
"}\n"
"\n"
"\n"
"\n"
"proc {info nameofexecutable} {} {\n"
"	if {[exists ::jim::exe]} {\n"
"		return $::jim::exe\n"
"	}\n"
"}\n"
"\n"
"\n"
"proc {dict update} {&varName args script} {\n"
"	set keys {}\n"
"	foreach {n v} $args {\n"
"		upvar $v var_$v\n"
"		if {[dict exists $varName $n]} {\n"
"			set var_$v [dict get $varName $n]\n"
"		}\n"
"	}\n"
"	catch {uplevel 1 $script} msg opts\n"
"	if {[info exists varName]} {\n"
"		foreach {n v} $args {\n"
"			if {[info exists var_$v]} {\n"
"				dict set varName $n [set var_$v]\n"
"			} else {\n"
"				dict unset varName $n\n"
"			}\n"
"		}\n"
"	}\n"
"	return {*}$opts $msg\n"
"}\n"
"\n"
"proc {dict replace} {dictionary {args {key value}}} {\n"
"	if {[llength ${key value}] % 2} {\n"
"		tailcall {dict replace}\n"
"	}\n"
"	tailcall dict merge $dictionary ${key value}\n"
"}\n"
"\n"
"\n"
"proc {dict lappend} {varName key {args value}} {\n"
"	upvar $varName dict\n"
"	if {[exists dict] && [dict exists $dict $key]} {\n"
"		set list [dict get $dict $key]\n"
"	}\n"
"	lappend list {*}$value\n"
"	dict set dict $key $list\n"
"}\n"
"\n"
"\n"
"proc {dict append} {varName key {args value}} {\n"
"	upvar $varName dict\n"
"	if {[exists dict] && [dict exists $dict $key]} {\n"
"		set str [dict get $dict $key]\n"
"	}\n"
"	append str {*}$value\n"
"	dict set dict $key $str\n"
"}\n"
"\n"
"\n"
"proc {dict incr} {varName key {increment 1}} {\n"
"	upvar $varName dict\n"
"	if {[exists dict] && [dict exists $dict $key]} {\n"
"		set value [dict get $dict $key]\n"
"	}\n"
"	incr value $increment\n"
"	dict set dict $key $value\n"
"}\n"
"\n"
"\n"
"proc {dict remove} {dictionary {args key}} {\n"
"	foreach k $key {\n"
"		dict unset dictionary $k\n"
"	}\n"
"	return $dictionary\n"
"}\n"
"\n"
"\n"
"proc {dict for} {vars dictionary script} {\n"
"	if {[llength $vars] != 2} {\n"
"		return -code error \"must have exactly two variable names\"\n"
"	}\n"
"	dict size $dictionary\n"
"	tailcall foreach $vars $dictionary $script\n"
"}\n"
);
}
int Jim_tclcompatInit(Jim_Interp *interp)
{
	if (Jim_PackageProvide(interp, "tclcompat", "1.0", JIM_ERRMSG))
		return JIM_ERR;

	return Jim_EvalSource(interp, "tclcompat.tcl", 1,
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"set env [env]\n"
"\n"
"\n"
"if {[info commands stdout] ne \"\"} {\n"
"\n"
"	foreach p {gets flush close eof seek tell} {\n"
"		proc $p {chan args} {p} {\n"
"			tailcall $chan $p {*}$args\n"
"		}\n"
"	}\n"
"	unset p\n"
"\n"
"\n"
"\n"
"	proc puts {{-nonewline {}} {chan stdout} msg} {\n"
"		if {${-nonewline} ni {-nonewline {}}} {\n"
"			tailcall ${-nonewline} puts $msg\n"
"		}\n"
"		tailcall $chan puts {*}${-nonewline} $msg\n"
"	}\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"	proc read {{-nonewline {}} chan} {\n"
"		if {${-nonewline} ni {-nonewline {}}} {\n"
"			tailcall ${-nonewline} read {*}${chan}\n"
"		}\n"
"		tailcall $chan read {*}${-nonewline}\n"
"	}\n"
"\n"
"	proc fconfigure {f args} {\n"
"		foreach {n v} $args {\n"
"			switch -glob -- $n {\n"
"				-bl* {\n"
"					$f ndelay $(!$v)\n"
"				}\n"
"				-bu* {\n"
"					$f buffering $v\n"
"				}\n"
"				-tr* {\n"
"\n"
"				}\n"
"				default {\n"
"					return -code error \"fconfigure: unknown option $n\"\n"
"				}\n"
"			}\n"
"		}\n"
"	}\n"
"}\n"
"\n"
"\n"
"proc fileevent {args} {\n"
"	tailcall {*}$args\n"
"}\n"
"\n"
"\n"
"\n"
"proc parray {arrayname {pattern *} {puts puts}} {\n"
"	upvar $arrayname a\n"
"\n"
"	set max 0\n"
"	foreach name [array names a $pattern]] {\n"
"		if {[string length $name] > $max} {\n"
"			set max [string length $name]\n"
"		}\n"
"	}\n"
"	incr max [string length $arrayname]\n"
"	incr max 2\n"
"	foreach name [lsort [array names a $pattern]] {\n"
"		$puts [format \"%-${max}s = %s\" $arrayname\\($name\\) $a($name)]\n"
"	}\n"
"}\n"
"\n"
"\n"
"proc {file copy} {{force {}} source target} {\n"
"	try {\n"
"		if {$force ni {{} -force}} {\n"
"			error \"bad option \\\"$force\\\": should be -force\"\n"
"		}\n"
"\n"
"		set in [open $source rb]\n"
"\n"
"		if {[file exists $target]} {\n"
"			if {$force eq \"\"} {\n"
"				error \"error copying \\\"$source\\\" to \\\"$target\\\": file already exists\"\n"
"			}\n"
"\n"
"			if {$source eq $target} {\n"
"				return\n"
"			}\n"
"\n"
"\n"
"			file stat $source ss\n"
"			file stat $target ts\n"
"			if {$ss(dev) == $ts(dev) && $ss(ino) == $ts(ino) && $ss(ino)} {\n"
"				return\n"
"			}\n"
"		}\n"
"		set out [open $target wb]\n"
"		$in copyto $out\n"
"		$out close\n"
"	} on error {msg opts} {\n"
"		incr opts(-level)\n"
"		return {*}$opts $msg\n"
"	} finally {\n"
"		catch {$in close}\n"
"	}\n"
"}\n"
"\n"
"\n"
"\n"
"proc popen {cmd {mode r}} {\n"
"	lassign [pipe] r w\n"
"	try {\n"
"		if {[string match \"w*\" $mode]} {\n"
"			lappend cmd <@$r &\n"
"			set pids [exec {*}$cmd]\n"
"			$r close\n"
"			set f $w\n"
"		} else {\n"
"			lappend cmd >@$w &\n"
"			set pids [exec {*}$cmd]\n"
"			$w close\n"
"			set f $r\n"
"		}\n"
"		lambda {cmd args} {f pids} {\n"
"			if {$cmd eq \"pid\"} {\n"
"				return $pids\n"
"			}\n"
"			if {$cmd eq \"getfd\"} {\n"
"				$f getfd\n"
"			}\n"
"			if {$cmd eq \"close\"} {\n"
"				$f close\n"
"\n"
"				set retopts {}\n"
"				foreach p $pids {\n"
"					lassign [wait $p] status - rc\n"
"					if {$status eq \"CHILDSTATUS\"} {\n"
"						if {$rc == 0} {\n"
"							continue\n"
"						}\n"
"						set msg \"child process exited abnormally\"\n"
"					} else {\n"
"						set msg \"child killed: received signal\"\n"
"					}\n"
"					set retopts [list -code error -errorcode [list $status $p $rc] $msg]\n"
"				}\n"
"				return {*}$retopts\n"
"			}\n"
"			tailcall $f $cmd {*}$args\n"
"		}\n"
"	} on error {error opts} {\n"
"		$r close\n"
"		$w close\n"
"		error $error\n"
"	}\n"
"}\n"
"\n"
"\n"
"local proc pid {{channelId {}}} {\n"
"	if {$channelId eq \"\"} {\n"
"		tailcall upcall pid\n"
"	}\n"
"	if {[catch {$channelId tell}]} {\n"
"		return -code error \"can not find channel named \\\"$channelId\\\"\"\n"
"	}\n"
"	if {[catch {$channelId pid} pids]} {\n"
"		return \"\"\n"
"	}\n"
"	return $pids\n"
"}\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"proc try {args} {\n"
"	set catchopts {}\n"
"	while {[string match -* [lindex $args 0]]} {\n"
"		set args [lassign $args opt]\n"
"		if {$opt eq \"--\"} {\n"
"			break\n"
"		}\n"
"		lappend catchopts $opt\n"
"	}\n"
"	if {[llength $args] == 0} {\n"
"		return -code error {wrong # args: should be \"try ?options? script ?argument ...?\"}\n"
"	}\n"
"	set args [lassign $args script]\n"
"	set code [catch -eval {*}$catchopts {uplevel 1 $script} msg opts]\n"
"\n"
"	set handled 0\n"
"\n"
"	foreach {on codes vars script} $args {\n"
"		switch -- $on \\\n"
"			on {\n"
"				if {!$handled && ($codes eq \"*\" || [info returncode $code] in $codes)} {\n"
"					lassign $vars msgvar optsvar\n"
"					if {$msgvar ne \"\"} {\n"
"						upvar $msgvar hmsg\n"
"						set hmsg $msg\n"
"					}\n"
"					if {$optsvar ne \"\"} {\n"
"						upvar $optsvar hopts\n"
"						set hopts $opts\n"
"					}\n"
"\n"
"					set code [catch {uplevel 1 $script} msg opts]\n"
"					incr handled\n"
"				}\n"
"			} \\\n"
"			finally {\n"
"				set finalcode [catch {uplevel 1 $codes} finalmsg finalopts]\n"
"				if {$finalcode} {\n"
"\n"
"					set code $finalcode\n"
"					set msg $finalmsg\n"
"					set opts $finalopts\n"
"				}\n"
"				break\n"
"			} \\\n"
"			default {\n"
"				return -code error \"try: expected 'on' or 'finally', got '$on'\"\n"
"			}\n"
"	}\n"
"\n"
"	if {$code} {\n"
"		incr opts(-level)\n"
"		return {*}$opts $msg\n"
"	}\n"
"	return $msg\n"
"}\n"
"\n"
"\n"
"\n"
"proc throw {code {msg \"\"}} {\n"
"	return -code $code $msg\n"
"}\n"
"\n"
"\n"
"proc {file delete force} {path} {\n"
"	foreach e [readdir $path] {\n"
"		file delete -force $path/$e\n"
"	}\n"
"	file delete $path\n"
"}\n"
);
}


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#include <sys/stat.h>
#endif


#if defined(HAVE_SYS_SOCKET_H) && defined(HAVE_SELECT) && defined(HAVE_NETINET_IN_H) && defined(HAVE_NETDB_H) && defined(HAVE_ARPA_INET_H)
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#define HAVE_SOCKETS
#elif defined (__MINGW32__)

#else
#define JIM_ANSIC
#endif

#if defined(JIM_SSL)
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#ifdef HAVE_TERMIOS_H
#endif


#define AIO_CMD_LEN 32
#define AIO_BUF_LEN 256

#ifndef HAVE_FTELLO
    #define ftello ftell
#endif
#ifndef HAVE_FSEEKO
    #define fseeko fseek
#endif

#define AIO_KEEPOPEN 1

#if defined(JIM_IPV6)
#define IPV6 1
#else
#define IPV6 0
#ifndef PF_INET6
#define PF_INET6 0
#endif
#endif

#ifdef JIM_ANSIC

#undef HAVE_PIPE
#undef HAVE_SOCKETPAIR
#endif


struct AioFile;

typedef struct {
    int (*writer)(struct AioFile *af, const char *buf, int len);
    int (*reader)(struct AioFile *af, char *buf, int len);
    const char *(*getline)(struct AioFile *af, char *buf, int len);
    int (*error)(const struct AioFile *af);
    const char *(*strerror)(struct AioFile *af);
    int (*verify)(struct AioFile *af);
} JimAioFopsType;

typedef struct AioFile
{
    FILE *fp;
    Jim_Obj *filename;
    int type;
    int openFlags;
    int fd;
    Jim_Obj *rEvent;
    Jim_Obj *wEvent;
    Jim_Obj *eEvent;
    int addr_family;
    void *ssl;
    const JimAioFopsType *fops;
} AioFile;

static int stdio_writer(struct AioFile *af, const char *buf, int len)
{
    return fwrite(buf, 1, len, af->fp);
}

static int stdio_reader(struct AioFile *af, char *buf, int len)
{
    return fread(buf, 1, len, af->fp);
}

static const char *stdio_getline(struct AioFile *af, char *buf, int len)
{
    return fgets(buf, len, af->fp);
}

static int stdio_error(const AioFile *af)
{
    if (!ferror(af->fp)) {
        return JIM_OK;
    }
    clearerr(af->fp);

    if (feof(af->fp) || errno == EAGAIN || errno == EINTR) {
        return JIM_OK;
    }
#ifdef ECONNRESET
    if (errno == ECONNRESET) {
        return JIM_OK;
    }
#endif
#ifdef ECONNABORTED
    if (errno == ECONNABORTED) {
        return JIM_OK;
    }
#endif
    return JIM_ERR;
}

static const char *stdio_strerror(struct AioFile *af)
{
    return strerror(errno);
}

static const JimAioFopsType stdio_fops = {
    stdio_writer,
    stdio_reader,
    stdio_getline,
    stdio_error,
    stdio_strerror,
    NULL
};


static int JimAioSubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv);
static AioFile *JimMakeChannel(Jim_Interp *interp, FILE *fh, int fd, Jim_Obj *filename,
    const char *hdlfmt, int family, const char *mode);


static const char *JimAioErrorString(AioFile *af)
{
    if (af && af->fops)
        return af->fops->strerror(af);

    return strerror(errno);
}

static void JimAioSetError(Jim_Interp *interp, Jim_Obj *name)
{
    AioFile *af = Jim_CmdPrivData(interp);

    if (name) {
        Jim_SetResultFormatted(interp, "%#s: %s", name, JimAioErrorString(af));
    }
    else {
        Jim_SetResultString(interp, JimAioErrorString(af), -1);
    }
}

static int JimCheckStreamError(Jim_Interp *interp, AioFile *af)
{
	int ret = af->fops->error(af);
	if (ret) {
		JimAioSetError(interp, af->filename);
	}
	return ret;
}

static void JimAioDelProc(Jim_Interp *interp, void *privData)
{
    AioFile *af = privData;

    JIM_NOTUSED(interp);

    Jim_DecrRefCount(interp, af->filename);

#ifdef jim_ext_eventloop

    Jim_DeleteFileHandler(interp, af->fd, JIM_EVENT_READABLE | JIM_EVENT_WRITABLE | JIM_EVENT_EXCEPTION);
#endif

#if defined(JIM_SSL)
    if (af->ssl != NULL) {
        SSL_free(af->ssl);
    }
#endif
    if (!(af->openFlags & AIO_KEEPOPEN)) {
        fclose(af->fp);
    }

    Jim_Free(af);
}

static int aio_cmd_read(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    char buf[AIO_BUF_LEN];
    Jim_Obj *objPtr;
    int nonewline = 0;
    jim_wide neededLen = -1;

    if (argc && Jim_CompareStringImmediate(interp, argv[0], "-nonewline")) {
        nonewline = 1;
        argv++;
        argc--;
    }
    if (argc == 1) {
        if (Jim_GetWide(interp, argv[0], &neededLen) != JIM_OK)
            return JIM_ERR;
        if (neededLen < 0) {
            Jim_SetResultString(interp, "invalid parameter: negative len", -1);
            return JIM_ERR;
        }
    }
    else if (argc) {
        return -1;
    }
    objPtr = Jim_NewStringObj(interp, NULL, 0);
    while (neededLen != 0) {
        int retval;
        int readlen;

        if (neededLen == -1) {
            readlen = AIO_BUF_LEN;
        }
        else {
            readlen = (neededLen > AIO_BUF_LEN ? AIO_BUF_LEN : neededLen);
        }
        retval = af->fops->reader(af, buf, readlen);
        if (retval > 0) {
            Jim_AppendString(interp, objPtr, buf, retval);
            if (neededLen != -1) {
                neededLen -= retval;
            }
        }
        if (retval != readlen)
            break;
    }

    if (JimCheckStreamError(interp, af)) {
        Jim_FreeNewObj(interp, objPtr);
        return JIM_ERR;
    }
    if (nonewline) {
        int len;
        const char *s = Jim_GetString(objPtr, &len);

        if (len > 0 && s[len - 1] == '\n') {
            objPtr->length--;
            objPtr->bytes[objPtr->length] = '\0';
        }
    }
    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}

AioFile *Jim_AioFile(Jim_Interp *interp, Jim_Obj *command)
{
    Jim_Cmd *cmdPtr = Jim_GetCommand(interp, command, JIM_ERRMSG);


    if (cmdPtr && !cmdPtr->isproc && cmdPtr->u.native.cmdProc == JimAioSubCmdProc) {
        return (AioFile *) cmdPtr->u.native.privData;
    }
    Jim_SetResultFormatted(interp, "Not a filehandle: \"%#s\"", command);
    return NULL;
}

FILE *Jim_AioFilehandle(Jim_Interp *interp, Jim_Obj *command)
{
    AioFile *af;

    af = Jim_AioFile(interp, command);
    if (af == NULL) {
        return NULL;
    }

    return af->fp;
}

static int aio_cmd_getfd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    fflush(af->fp);
    Jim_SetResultInt(interp, fileno(af->fp));

    return JIM_OK;
}

static int aio_cmd_copy(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    jim_wide count = 0;
    jim_wide maxlen = JIM_WIDE_MAX;
    AioFile *outf = Jim_AioFile(interp, argv[0]);

    if (outf == NULL) {
        return JIM_ERR;
    }

    if (argc == 2) {
        if (Jim_GetWide(interp, argv[1], &maxlen) != JIM_OK) {
            return JIM_ERR;
        }
    }

    while (count < maxlen) {
        char ch;

        if (af->fops->reader(af, &ch, 1) != 1) {
            break;
        }
        if (outf->fops->writer(outf, &ch, 1) != 1) {
            break;
        }
        count++;
    }

    if (JimCheckStreamError(interp, af) || JimCheckStreamError(interp, outf)) {
        return JIM_ERR;
    }

    Jim_SetResultInt(interp, count);

    return JIM_OK;
}

static int aio_cmd_gets(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    char buf[AIO_BUF_LEN];
    Jim_Obj *objPtr;
    int len;

    errno = 0;

    objPtr = Jim_NewStringObj(interp, NULL, 0);
    while (1) {
        buf[AIO_BUF_LEN - 1] = '_';

        if (af->fops->getline(af, buf, AIO_BUF_LEN) == NULL)
            break;

        if (buf[AIO_BUF_LEN - 1] == '\0' && buf[AIO_BUF_LEN - 2] != '\n') {
            Jim_AppendString(interp, objPtr, buf, AIO_BUF_LEN - 1);
        }
        else {
            len = strlen(buf);

            if (len && (buf[len - 1] == '\n')) {

                len--;
            }

            Jim_AppendString(interp, objPtr, buf, len);
            break;
        }
    }

    if (JimCheckStreamError(interp, af)) {

        Jim_FreeNewObj(interp, objPtr);
        return JIM_ERR;
    }

    if (argc) {
        if (Jim_SetVariable(interp, argv[0], objPtr) != JIM_OK) {
            Jim_FreeNewObj(interp, objPtr);
            return JIM_ERR;
        }

        len = Jim_Length(objPtr);

        if (len == 0 && feof(af->fp)) {

            len = -1;
        }
        Jim_SetResultInt(interp, len);
    }
    else {
        Jim_SetResult(interp, objPtr);
    }
    return JIM_OK;
}

static int aio_cmd_puts(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    int wlen;
    const char *wdata;
    Jim_Obj *strObj;

    if (argc == 2) {
        if (!Jim_CompareStringImmediate(interp, argv[0], "-nonewline")) {
            return -1;
        }
        strObj = argv[1];
    }
    else {
        strObj = argv[0];
    }

    wdata = Jim_GetString(strObj, &wlen);
    if (af->fops->writer(af, wdata, wlen) == wlen) {
        if (argc == 2 || af->fops->writer(af, "\n", 1) == 1) {
            return JIM_OK;
        }
    }
    JimAioSetError(interp, af->filename);
    return JIM_ERR;
}

static int aio_cmd_isatty(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
#ifdef HAVE_ISATTY
    AioFile *af = Jim_CmdPrivData(interp);
    Jim_SetResultInt(interp, isatty(fileno(af->fp)));
#else
    Jim_SetResultInt(interp, 0);
#endif

    return JIM_OK;
}


static int aio_cmd_flush(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    if (fflush(af->fp) == EOF) {
        JimAioSetError(interp, af->filename);
        return JIM_ERR;
    }
    return JIM_OK;
}

static int aio_cmd_eof(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    Jim_SetResultInt(interp, feof(af->fp));
    return JIM_OK;
}

static int aio_cmd_close(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc == 3) {
#if defined(HAVE_SOCKETS) && defined(HAVE_SHUTDOWN)
        static const char * const options[] = { "r", "w", NULL };
        enum { OPT_R, OPT_W, };
        int option;
        AioFile *af = Jim_CmdPrivData(interp);

        if (Jim_GetEnum(interp, argv[2], options, &option, NULL, JIM_ERRMSG) != JIM_OK) {
            return JIM_ERR;
        }
        if (shutdown(af->fd, option == OPT_R ? SHUT_RD : SHUT_WR) == 0) {
            return JIM_OK;
        }
        JimAioSetError(interp, NULL);
#else
        Jim_SetResultString(interp, "async close not supported", -1);
#endif
        return JIM_ERR;
    }

    return Jim_DeleteCommand(interp, Jim_String(argv[0]));
}

static int aio_cmd_seek(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    int orig = SEEK_SET;
    jim_wide offset;

    if (argc == 2) {
        if (Jim_CompareStringImmediate(interp, argv[1], "start"))
            orig = SEEK_SET;
        else if (Jim_CompareStringImmediate(interp, argv[1], "current"))
            orig = SEEK_CUR;
        else if (Jim_CompareStringImmediate(interp, argv[1], "end"))
            orig = SEEK_END;
        else {
            return -1;
        }
    }
    if (Jim_GetWide(interp, argv[0], &offset) != JIM_OK) {
        return JIM_ERR;
    }
    if (fseeko(af->fp, offset, orig) == -1) {
        JimAioSetError(interp, af->filename);
        return JIM_ERR;
    }
    return JIM_OK;
}

static int aio_cmd_tell(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    Jim_SetResultInt(interp, ftello(af->fp));
    return JIM_OK;
}

static int aio_cmd_filename(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    Jim_SetResult(interp, af->filename);
    return JIM_OK;
}

#ifdef O_NDELAY
static int aio_cmd_ndelay(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    int fmode = fcntl(af->fd, F_GETFL);

    if (argc) {
        long nb;

        if (Jim_GetLong(interp, argv[0], &nb) != JIM_OK) {
            return JIM_ERR;
        }
        if (nb) {
            fmode |= O_NDELAY;
        }
        else {
            fmode &= ~O_NDELAY;
        }
        (void)fcntl(af->fd, F_SETFL, fmode);
    }
    Jim_SetResultInt(interp, (fmode & O_NONBLOCK) ? 1 : 0);
    return JIM_OK;
}
#endif


#ifdef HAVE_FSYNC
static int aio_cmd_sync(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    fflush(af->fp);
    fsync(af->fd);
    return JIM_OK;
}
#endif

static int aio_cmd_buffering(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    static const char * const options[] = {
        "none",
        "line",
        "full",
        NULL
    };
    enum
    {
        OPT_NONE,
        OPT_LINE,
        OPT_FULL,
    };
    int option;

    if (Jim_GetEnum(interp, argv[0], options, &option, NULL, JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }
    switch (option) {
        case OPT_NONE:
            setvbuf(af->fp, NULL, _IONBF, 0);
            break;
        case OPT_LINE:
            setvbuf(af->fp, NULL, _IOLBF, BUFSIZ);
            break;
        case OPT_FULL:
            setvbuf(af->fp, NULL, _IOFBF, BUFSIZ);
            break;
    }
    return JIM_OK;
}

#ifdef jim_ext_eventloop
static void JimAioFileEventFinalizer(Jim_Interp *interp, void *clientData)
{
    Jim_Obj **objPtrPtr = clientData;

    Jim_DecrRefCount(interp, *objPtrPtr);
    *objPtrPtr = NULL;
}

static int JimAioFileEventHandler(Jim_Interp *interp, void *clientData, int mask)
{
    Jim_Obj **objPtrPtr = clientData;

    return Jim_EvalObjBackground(interp, *objPtrPtr);
}

static int aio_eventinfo(Jim_Interp *interp, AioFile * af, unsigned mask, Jim_Obj **scriptHandlerObj,
    int argc, Jim_Obj * const *argv)
{
    if (argc == 0) {

        if (*scriptHandlerObj) {
            Jim_SetResult(interp, *scriptHandlerObj);
        }
        return JIM_OK;
    }

    if (*scriptHandlerObj) {

        Jim_DeleteFileHandler(interp, af->fd, mask);
    }


    if (Jim_Length(argv[0]) == 0) {

        return JIM_OK;
    }


    Jim_IncrRefCount(argv[0]);
    *scriptHandlerObj = argv[0];

    Jim_CreateFileHandler(interp, af->fd, mask,
        JimAioFileEventHandler, scriptHandlerObj, JimAioFileEventFinalizer);

    return JIM_OK;
}

static int aio_cmd_readable(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    return aio_eventinfo(interp, af, JIM_EVENT_READABLE, &af->rEvent, argc, argv);
}

static int aio_cmd_writable(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    return aio_eventinfo(interp, af, JIM_EVENT_WRITABLE, &af->wEvent, argc, argv);
}

static int aio_cmd_onexception(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    return aio_eventinfo(interp, af, JIM_EVENT_EXCEPTION, &af->eEvent, argc, argv);
}
#endif




static const jim_subcmd_type aio_command_table[] = {
    {   "read",
        "?-nonewline? ?len?",
        aio_cmd_read,
        0,
        2,

    },
    {   "copyto",
        "handle ?size?",
        aio_cmd_copy,
        1,
        2,

    },
    {   "getfd",
        NULL,
        aio_cmd_getfd,
        0,
        0,

    },
    {   "gets",
        "?var?",
        aio_cmd_gets,
        0,
        1,

    },
    {   "puts",
        "?-nonewline? str",
        aio_cmd_puts,
        1,
        2,

    },
    {   "isatty",
        NULL,
        aio_cmd_isatty,
        0,
        0,

    },
    {   "flush",
        NULL,
        aio_cmd_flush,
        0,
        0,

    },
    {   "eof",
        NULL,
        aio_cmd_eof,
        0,
        0,

    },
    {   "close",
        "?r(ead)|w(rite)?",
        aio_cmd_close,
        0,
        1,
        JIM_MODFLAG_FULLARGV,

    },
    {   "seek",
        "offset ?start|current|end",
        aio_cmd_seek,
        1,
        2,

    },
    {   "tell",
        NULL,
        aio_cmd_tell,
        0,
        0,

    },
    {   "filename",
        NULL,
        aio_cmd_filename,
        0,
        0,

    },
#ifdef O_NDELAY
    {   "ndelay",
        "?0|1?",
        aio_cmd_ndelay,
        0,
        1,

    },
#endif
#ifdef HAVE_FSYNC
    {   "sync",
        NULL,
        aio_cmd_sync,
        0,
        0,

    },
#endif
    {   "buffering",
        "none|line|full",
        aio_cmd_buffering,
        1,
        1,

    },
#ifdef jim_ext_eventloop
    {   "readable",
        "?readable-script?",
        aio_cmd_readable,
        0,
        1,

    },
    {   "writable",
        "?writable-script?",
        aio_cmd_writable,
        0,
        1,

    },
    {   "onexception",
        "?exception-script?",
        aio_cmd_onexception,
        0,
        1,

    },
#endif
    { NULL }
};

static int JimAioSubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_CallSubCmd(interp, Jim_ParseSubCmd(interp, aio_command_table, argc, argv), argc, argv);
}

static int JimAioOpenCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    const char *mode;

    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "filename ?mode?");
        return JIM_ERR;
    }

    mode = (argc == 3) ? Jim_String(argv[2]) : "r";

#ifdef jim_ext_tclcompat
    {
        const char *filename = Jim_String(argv[1]);


        if (*filename == '|') {
            Jim_Obj *evalObj[3];

            evalObj[0] = Jim_NewStringObj(interp, "::popen", -1);
            evalObj[1] = Jim_NewStringObj(interp, filename + 1, -1);
            evalObj[2] = Jim_NewStringObj(interp, mode, -1);

            return Jim_EvalObjVector(interp, 3, evalObj);
        }
    }
#endif
    return JimMakeChannel(interp, NULL, -1, argv[1], "aio.handle%ld", 0, mode) ? JIM_OK : JIM_ERR;
}


static AioFile *JimMakeChannel(Jim_Interp *interp, FILE *fh, int fd, Jim_Obj *filename,
    const char *hdlfmt, int family, const char *mode)
{
    AioFile *af;
    char buf[AIO_CMD_LEN];
    int openFlags = 0;

    snprintf(buf, sizeof(buf), hdlfmt, Jim_GetId(interp));

    if (fh) {
        openFlags = AIO_KEEPOPEN;
    }

    snprintf(buf, sizeof(buf), hdlfmt, Jim_GetId(interp));
    if (!filename) {
        filename = Jim_NewStringObj(interp, buf, -1);
    }

    Jim_IncrRefCount(filename);

    if (fh == NULL) {
        if (fd >= 0) {
#ifndef JIM_ANSIC
            fh = fdopen(fd, mode);
#endif
        }
        else
            fh = fopen(Jim_String(filename), mode);

        if (fh == NULL) {
            JimAioSetError(interp, filename);
#ifndef JIM_ANSIC
            if (fd >= 0) {
                close(fd);
            }
#endif
            Jim_DecrRefCount(interp, filename);
            return NULL;
        }
    }


    af = Jim_Alloc(sizeof(*af));
    memset(af, 0, sizeof(*af));
    af->fp = fh;
    af->filename = filename;
    af->openFlags = openFlags;
#ifndef JIM_ANSIC
    af->fd = fileno(fh);
#ifdef FD_CLOEXEC
    if ((openFlags & AIO_KEEPOPEN) == 0) {
        (void)fcntl(af->fd, F_SETFD, FD_CLOEXEC);
    }
#endif
#endif
    af->addr_family = family;
    af->fops = &stdio_fops;
    af->ssl = NULL;

    Jim_CreateCommand(interp, buf, JimAioSubCmdProc, af, JimAioDelProc);

    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));

    return af;
}

#if defined(HAVE_PIPE) || (defined(HAVE_SOCKETPAIR) && defined(HAVE_SYS_UN_H))
static int JimMakeChannelPair(Jim_Interp *interp, int p[2], Jim_Obj *filename,
    const char *hdlfmt, int family, const char *mode[2])
{
    if (JimMakeChannel(interp, NULL, p[0], filename, hdlfmt, family, mode[0])) {
        Jim_Obj *objPtr = Jim_NewListObj(interp, NULL, 0);
        Jim_ListAppendElement(interp, objPtr, Jim_GetResult(interp));
        if (JimMakeChannel(interp, NULL, p[1], filename, hdlfmt, family, mode[1])) {
            Jim_ListAppendElement(interp, objPtr, Jim_GetResult(interp));
            Jim_SetResult(interp, objPtr);
            return JIM_OK;
        }
    }


    close(p[0]);
    close(p[1]);
    JimAioSetError(interp, NULL);
    return JIM_ERR;
}
#endif

#ifdef HAVE_PIPE
static int JimAioPipeCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int p[2];
    static const char *mode[2] = { "r", "w" };

    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }

    if (pipe(p) != 0) {
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }

    return JimMakeChannelPair(interp, p, argv[0], "aio.pipe%ld", 0, mode);
}
#endif



int Jim_aioInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "aio", "1.0", JIM_ERRMSG))
        return JIM_ERR;

#if defined(JIM_SSL)
    Jim_CreateCommand(interp, "load_ssl_certs", JimAioLoadSSLCertsCommand, NULL, NULL);
#endif

    Jim_CreateCommand(interp, "open", JimAioOpenCommand, NULL, NULL);
#ifdef HAVE_SOCKETS
    Jim_CreateCommand(interp, "socket", JimAioSockCommand, NULL, NULL);
#endif
#ifdef HAVE_PIPE
    Jim_CreateCommand(interp, "pipe", JimAioPipeCommand, NULL, NULL);
#endif


    JimMakeChannel(interp, stdin, -1, NULL, "stdin", 0, "r");
    JimMakeChannel(interp, stdout, -1, NULL, "stdout", 0, "w");
    JimMakeChannel(interp, stderr, -1, NULL, "stderr", 0, "w");

    return JIM_OK;
}

#include <errno.h>
#include <stdio.h>
#include <string.h>


#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

int Jim_ReaddirCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *dirPath;
    DIR *dirPtr;
    struct dirent *entryPtr;
    int nocomplain = 0;

    if (argc == 3 && Jim_CompareStringImmediate(interp, argv[1], "-nocomplain")) {
        nocomplain = 1;
    }
    if (argc != 2 && !nocomplain) {
        Jim_WrongNumArgs(interp, 1, argv, "?-nocomplain? dirPath");
        return JIM_ERR;
    }

    dirPath = Jim_String(argv[1 + nocomplain]);

    dirPtr = opendir(dirPath);
    if (dirPtr == NULL) {
        if (nocomplain) {
            return JIM_OK;
        }
        Jim_SetResultString(interp, strerror(errno), -1);
        return JIM_ERR;
    }
    else {
        Jim_Obj *listObj = Jim_NewListObj(interp, NULL, 0);

        while ((entryPtr = readdir(dirPtr)) != NULL) {
            if (entryPtr->d_name[0] == '.') {
                if (entryPtr->d_name[1] == '\0') {
                    continue;
                }
                if ((entryPtr->d_name[1] == '.') && (entryPtr->d_name[2] == '\0'))
                    continue;
            }
            Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp, entryPtr->d_name, -1));
        }
        closedir(dirPtr);

        Jim_SetResult(interp, listObj);

        return JIM_OK;
    }
}

int Jim_readdirInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "readdir", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "readdir", Jim_ReaddirCmd, NULL, NULL);
    return JIM_OK;
}

#include <stdlib.h>
#include <string.h>

#if defined(JIM_REGEXP)
#else
    #include <regex.h>
#endif

static void FreeRegexpInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    regfree(objPtr->internalRep.ptrIntValue.ptr);
    Jim_Free(objPtr->internalRep.ptrIntValue.ptr);
}

static const Jim_ObjType regexpObjType = {
    "regexp",
    FreeRegexpInternalRep,
    NULL,
    NULL,
    JIM_TYPE_NONE
};

static regex_t *SetRegexpFromAny(Jim_Interp *interp, Jim_Obj *objPtr, unsigned flags)
{
    regex_t *compre;
    const char *pattern;
    int ret;


    if (objPtr->typePtr == &regexpObjType &&
        objPtr->internalRep.ptrIntValue.ptr && objPtr->internalRep.ptrIntValue.int1 == flags) {

        return objPtr->internalRep.ptrIntValue.ptr;
    }




    pattern = Jim_String(objPtr);
    compre = Jim_Alloc(sizeof(regex_t));

    if ((ret = regcomp(compre, pattern, REG_EXTENDED | flags)) != 0) {
        char buf[100];

        regerror(ret, compre, buf, sizeof(buf));
        Jim_SetResultFormatted(interp, "couldn't compile regular expression pattern: %s", buf);
        regfree(compre);
        Jim_Free(compre);
        return NULL;
    }

    Jim_FreeIntRep(interp, objPtr);

    objPtr->typePtr = &regexpObjType;
    objPtr->internalRep.ptrIntValue.int1 = flags;
    objPtr->internalRep.ptrIntValue.ptr = compre;

    return compre;
}

int Jim_RegexpCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int opt_indices = 0;
    int opt_all = 0;
    int opt_inline = 0;
    regex_t *regex;
    int match, i, j;
    int offset = 0;
    regmatch_t *pmatch = NULL;
    int source_len;
    int result = JIM_OK;
    const char *pattern;
    const char *source_str;
    int num_matches = 0;
    int num_vars;
    Jim_Obj *resultListObj = NULL;
    int regcomp_flags = 0;
    int eflags = 0;
    int option;
    enum {
        OPT_INDICES,  OPT_NOCASE, OPT_LINE, OPT_ALL, OPT_INLINE, OPT_START, OPT_END
    };
    static const char * const options[] = {
        "-indices", "-nocase", "-line", "-all", "-inline", "-start", "--", NULL
    };

    if (argc < 3) {
      wrongNumArgs:
        Jim_WrongNumArgs(interp, 1, argv,
            "?-switch ...? exp string ?matchVar? ?subMatchVar ...?");
        return JIM_ERR;
    }

    for (i = 1; i < argc; i++) {
        const char *opt = Jim_String(argv[i]);

        if (*opt != '-') {
            break;
        }
        if (Jim_GetEnum(interp, argv[i], options, &option, "switch", JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
            return JIM_ERR;
        }
        if (option == OPT_END) {
            i++;
            break;
        }
        switch (option) {
            case OPT_INDICES:
                opt_indices = 1;
                break;

            case OPT_NOCASE:
                regcomp_flags |= REG_ICASE;
                break;

            case OPT_LINE:
                regcomp_flags |= REG_NEWLINE;
                break;

            case OPT_ALL:
                opt_all = 1;
                break;

            case OPT_INLINE:
                opt_inline = 1;
                break;

            case OPT_START:
                if (++i == argc) {
                    goto wrongNumArgs;
                }
                if (Jim_GetIndex(interp, argv[i], &offset) != JIM_OK) {
                    return JIM_ERR;
                }
                break;
        }
    }
    if (argc - i < 2) {
        goto wrongNumArgs;
    }

    regex = SetRegexpFromAny(interp, argv[i], regcomp_flags);
    if (!regex) {
        return JIM_ERR;
    }

    pattern = Jim_String(argv[i]);
    source_str = Jim_GetString(argv[i + 1], &source_len);

    num_vars = argc - i - 2;

    if (opt_inline) {
        if (num_vars) {
            Jim_SetResultString(interp, "regexp match variables not allowed when using -inline",
                -1);
            result = JIM_ERR;
            goto done;
        }
        num_vars = regex->re_nsub + 1;
    }

    pmatch = Jim_Alloc((num_vars + 1) * sizeof(*pmatch));

    if (offset) {
        if (offset < 0) {
            offset += source_len + 1;
        }
        if (offset > source_len) {
            source_str += source_len;
        }
        else if (offset > 0) {
            source_str += offset;
        }
        eflags |= REG_NOTBOL;
    }

    if (opt_inline) {
        resultListObj = Jim_NewListObj(interp, NULL, 0);
    }

  next_match:
    match = regexec(regex, source_str, num_vars + 1, pmatch, eflags);
    if (match >= REG_BADPAT) {
        char buf[100];

        regerror(match, regex, buf, sizeof(buf));
        Jim_SetResultFormatted(interp, "error while matching pattern: %s", buf);
        result = JIM_ERR;
        goto done;
    }

    if (match == REG_NOMATCH) {
        goto done;
    }

    num_matches++;

    if (opt_all && !opt_inline) {

        goto try_next_match;
    }


    j = 0;
    for (i += 2; opt_inline ? j < num_vars : i < argc; i++, j++) {
        Jim_Obj *resultObj;

        if (opt_indices) {
            resultObj = Jim_NewListObj(interp, NULL, 0);
        }
        else {
            resultObj = Jim_NewStringObj(interp, "", 0);
        }

        if (pmatch[j].rm_so == -1) {
            if (opt_indices) {
                Jim_ListAppendElement(interp, resultObj, Jim_NewIntObj(interp, -1));
                Jim_ListAppendElement(interp, resultObj, Jim_NewIntObj(interp, -1));
            }
        }
        else {
            int len = pmatch[j].rm_eo - pmatch[j].rm_so;

            if (opt_indices) {
                Jim_ListAppendElement(interp, resultObj, Jim_NewIntObj(interp,
                        offset + pmatch[j].rm_so));
                Jim_ListAppendElement(interp, resultObj, Jim_NewIntObj(interp,
                        offset + pmatch[j].rm_so + len - 1));
            }
            else {
                Jim_AppendString(interp, resultObj, source_str + pmatch[j].rm_so, len);
            }
        }

        if (opt_inline) {
            Jim_ListAppendElement(interp, resultListObj, resultObj);
        }
        else {

            result = Jim_SetVariable(interp, argv[i], resultObj);

            if (result != JIM_OK) {
                Jim_FreeObj(interp, resultObj);
                break;
            }
        }
    }

  try_next_match:
    if (opt_all && (pattern[0] != '^' || (regcomp_flags & REG_NEWLINE)) && *source_str) {
        if (pmatch[0].rm_eo) {
            offset += pmatch[0].rm_eo;
            source_str += pmatch[0].rm_eo;
        }
        else {
            source_str++;
            offset++;
        }
        if (*source_str) {
            eflags = REG_NOTBOL;
            goto next_match;
        }
    }

  done:
    if (result == JIM_OK) {
        if (opt_inline) {
            Jim_SetResult(interp, resultListObj);
        }
        else {
            Jim_SetResultInt(interp, num_matches);
        }
    }

    Jim_Free(pmatch);
    return result;
}

#define MAX_SUB_MATCHES 50

int Jim_RegsubCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int regcomp_flags = 0;
    int regexec_flags = 0;
    int opt_all = 0;
    int offset = 0;
    regex_t *regex;
    const char *p;
    int result;
    regmatch_t pmatch[MAX_SUB_MATCHES + 1];
    int num_matches = 0;

    int i, j, n;
    Jim_Obj *varname;
    Jim_Obj *resultObj;
    const char *source_str;
    int source_len;
    const char *replace_str;
    int replace_len;
    const char *pattern;
    int option;
    enum {
        OPT_NOCASE, OPT_LINE, OPT_ALL, OPT_START, OPT_END
    };
    static const char * const options[] = {
        "-nocase", "-line", "-all", "-start", "--", NULL
    };

    if (argc < 4) {
      wrongNumArgs:
        Jim_WrongNumArgs(interp, 1, argv,
            "?-switch ...? exp string subSpec ?varName?");
        return JIM_ERR;
    }

    for (i = 1; i < argc; i++) {
        const char *opt = Jim_String(argv[i]);

        if (*opt != '-') {
            break;
        }
        if (Jim_GetEnum(interp, argv[i], options, &option, "switch", JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
            return JIM_ERR;
        }
        if (option == OPT_END) {
            i++;
            break;
        }
        switch (option) {
            case OPT_NOCASE:
                regcomp_flags |= REG_ICASE;
                break;

            case OPT_LINE:
                regcomp_flags |= REG_NEWLINE;
                break;

            case OPT_ALL:
                opt_all = 1;
                break;

            case OPT_START:
                if (++i == argc) {
                    goto wrongNumArgs;
                }
                if (Jim_GetIndex(interp, argv[i], &offset) != JIM_OK) {
                    return JIM_ERR;
                }
                break;
        }
    }
    if (argc - i != 3 && argc - i != 4) {
        goto wrongNumArgs;
    }

    regex = SetRegexpFromAny(interp, argv[i], regcomp_flags);
    if (!regex) {
        return JIM_ERR;
    }
    pattern = Jim_String(argv[i]);

    source_str = Jim_GetString(argv[i + 1], &source_len);
    replace_str = Jim_GetString(argv[i + 2], &replace_len);
    varname = argv[i + 3];


    resultObj = Jim_NewStringObj(interp, "", 0);

    if (offset) {
        if (offset < 0) {
            offset += source_len + 1;
        }
        if (offset > source_len) {
            offset = source_len;
        }
        else if (offset < 0) {
            offset = 0;
        }
    }


    Jim_AppendString(interp, resultObj, source_str, offset);


    n = source_len - offset;
    p = source_str + offset;
    do {
        int match = regexec(regex, p, MAX_SUB_MATCHES, pmatch, regexec_flags);

        if (match >= REG_BADPAT) {
            char buf[100];

            regerror(match, regex, buf, sizeof(buf));
            Jim_SetResultFormatted(interp, "error while matching pattern: %s", buf);
            return JIM_ERR;
        }
        if (match == REG_NOMATCH) {
            break;
        }

        num_matches++;

        Jim_AppendString(interp, resultObj, p, pmatch[0].rm_so);


        for (j = 0; j < replace_len; j++) {
            int idx;
            int c = replace_str[j];

            if (c == '&') {
                idx = 0;
            }
            else if (c == '\\' && j < replace_len) {
                c = replace_str[++j];
                if ((c >= '0') && (c <= '9')) {
                    idx = c - '0';
                }
                else if ((c == '\\') || (c == '&')) {
                    Jim_AppendString(interp, resultObj, replace_str + j, 1);
                    continue;
                }
                else {
                    Jim_AppendString(interp, resultObj, replace_str + j - 1, (j == replace_len) ? 1 : 2);
                    continue;
                }
            }
            else {
                Jim_AppendString(interp, resultObj, replace_str + j, 1);
                continue;
            }
            if ((idx < MAX_SUB_MATCHES) && pmatch[idx].rm_so != -1 && pmatch[idx].rm_eo != -1) {
                Jim_AppendString(interp, resultObj, p + pmatch[idx].rm_so,
                    pmatch[idx].rm_eo - pmatch[idx].rm_so);
            }
        }

        p += pmatch[0].rm_eo;
        n -= pmatch[0].rm_eo;


        if (!opt_all || n == 0) {
            break;
        }


        if ((regcomp_flags & REG_NEWLINE) == 0 && pattern[0] == '^') {
            break;
        }


        if (pattern[0] == '\0' && n) {

            Jim_AppendString(interp, resultObj, p, 1);
            p++;
            n--;
        }

        regexec_flags |= REG_NOTBOL;
    } while (n);

    Jim_AppendString(interp, resultObj, p, -1);


    if (argc - i == 4) {
        result = Jim_SetVariable(interp, varname, resultObj);

        if (result == JIM_OK) {
            Jim_SetResultInt(interp, num_matches);
        }
        else {
            Jim_FreeObj(interp, resultObj);
        }
    }
    else {
        Jim_SetResult(interp, resultObj);
        result = JIM_OK;
    }

    return result;
}

int Jim_regexpInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "regexp", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "regexp", Jim_RegexpCmd, NULL, NULL);
    Jim_CreateCommand(interp, "regsub", Jim_RegsubCmd, NULL, NULL);
    return JIM_OK;
}

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>


#ifdef HAVE_UTIMES
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#elif defined(_MSC_VER)
#include <direct.h>
#define F_OK 0
#define W_OK 2
#define R_OK 4
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

# ifndef MAXPATHLEN
# define MAXPATHLEN JIM_PATH_LEN
# endif

#if defined(__MINGW32__) || defined(__MSYS__) || defined(_MSC_VER)
#define ISWINDOWS 1
#else
#define ISWINDOWS 0
#endif


#if defined(HAVE_STRUCT_STAT_ST_MTIMESPEC)
    #define STAT_MTIME_US(STAT) ((STAT).st_mtimespec.tv_sec * 1000000ll + (STAT).st_mtimespec.tv_nsec / 1000)
#elif defined(HAVE_STRUCT_STAT_ST_MTIM)
    #define STAT_MTIME_US(STAT) ((STAT).st_mtim.tv_sec * 1000000ll + (STAT).st_mtim.tv_nsec / 1000)
#endif


static const char *JimGetFileType(int mode)
{
    if (S_ISREG(mode)) {
        return "file";
    }
    else if (S_ISDIR(mode)) {
        return "directory";
    }
#ifdef S_ISCHR
    else if (S_ISCHR(mode)) {
        return "characterSpecial";
    }
#endif
#ifdef S_ISBLK
    else if (S_ISBLK(mode)) {
        return "blockSpecial";
    }
#endif
#ifdef S_ISFIFO
    else if (S_ISFIFO(mode)) {
        return "fifo";
    }
#endif
#ifdef S_ISLNK
    else if (S_ISLNK(mode)) {
        return "link";
    }
#endif
#ifdef S_ISSOCK
    else if (S_ISSOCK(mode)) {
        return "socket";
    }
#endif
    return "unknown";
}

static void AppendStatElement(Jim_Interp *interp, Jim_Obj *listObj, const char *key, jim_wide value)
{
    Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp, key, -1));
    Jim_ListAppendElement(interp, listObj, Jim_NewIntObj(interp, value));
}

static int StoreStatData(Jim_Interp *interp, Jim_Obj *varName, const struct stat *sb)
{

    Jim_Obj *listObj = Jim_NewListObj(interp, NULL, 0);

    AppendStatElement(interp, listObj, "dev", sb->st_dev);
    AppendStatElement(interp, listObj, "ino", sb->st_ino);
    AppendStatElement(interp, listObj, "mode", sb->st_mode);
    AppendStatElement(interp, listObj, "nlink", sb->st_nlink);
    AppendStatElement(interp, listObj, "uid", sb->st_uid);
    AppendStatElement(interp, listObj, "gid", sb->st_gid);
    AppendStatElement(interp, listObj, "size", sb->st_size);
    AppendStatElement(interp, listObj, "atime", sb->st_atime);
    AppendStatElement(interp, listObj, "mtime", sb->st_mtime);
    AppendStatElement(interp, listObj, "ctime", sb->st_ctime);
#ifdef STAT_MTIME_US
    AppendStatElement(interp, listObj, "mtimeus", STAT_MTIME_US(*sb));
#endif
    Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp, "type", -1));
    Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp, JimGetFileType((int)sb->st_mode), -1));


    if (varName) {
        Jim_Obj *objPtr;
        objPtr = Jim_GetVariable(interp, varName, JIM_NONE);

        if (objPtr) {
            Jim_Obj *objv[2];

            objv[0] = objPtr;
            objv[1] = listObj;

            objPtr = Jim_DictMerge(interp, 2, objv);
            if (objPtr == NULL) {

                Jim_SetResultFormatted(interp, "can't set \"%#s(dev)\": variable isn't array", varName);
                Jim_FreeNewObj(interp, listObj);
                return JIM_ERR;
            }

            Jim_InvalidateStringRep(objPtr);

            Jim_FreeNewObj(interp, listObj);
            listObj = objPtr;
        }
        Jim_SetVariable(interp, varName, listObj);
    }


    Jim_SetResult(interp, listObj);

    return JIM_OK;
}

static int file_cmd_dirname(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *path = Jim_String(argv[0]);
    const char *p = strrchr(path, '/');

    if (!p && path[0] == '.' && path[1] == '.' && path[2] == '\0') {
        Jim_SetResultString(interp, "..", -1);
    } else if (!p) {
        Jim_SetResultString(interp, ".", -1);
    }
    else if (p == path) {
        Jim_SetResultString(interp, "/", -1);
    }
    else if (ISWINDOWS && p[-1] == ':') {

        Jim_SetResultString(interp, path, p - path + 1);
    }
    else {
        Jim_SetResultString(interp, path, p - path);
    }
    return JIM_OK;
}

static int file_cmd_rootname(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *path = Jim_String(argv[0]);
    const char *lastSlash = strrchr(path, '/');
    const char *p = strrchr(path, '.');

    if (p == NULL || (lastSlash != NULL && lastSlash > p)) {
        Jim_SetResult(interp, argv[0]);
    }
    else {
        Jim_SetResultString(interp, path, p - path);
    }
    return JIM_OK;
}

static int file_cmd_extension(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *path = Jim_String(argv[0]);
    const char *lastSlash = strrchr(path, '/');
    const char *p = strrchr(path, '.');

    if (p == NULL || (lastSlash != NULL && lastSlash >= p)) {
        p = "";
    }
    Jim_SetResultString(interp, p, -1);
    return JIM_OK;
}

static int file_cmd_tail(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *path = Jim_String(argv[0]);
    const char *lastSlash = strrchr(path, '/');

    if (lastSlash) {
        Jim_SetResultString(interp, lastSlash + 1, -1);
    }
    else {
        Jim_SetResult(interp, argv[0]);
    }
    return JIM_OK;
}

static int file_cmd_normalize(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
#ifdef HAVE_REALPATH
    const char *path = Jim_String(argv[0]);
    char *newname = Jim_Alloc(MAXPATHLEN + 1);

    if (realpath(path, newname)) {
        Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, newname, -1));
        return JIM_OK;
    }
    else {
        Jim_Free(newname);
        Jim_SetResultFormatted(interp, "can't normalize \"%#s\": %s", argv[0], strerror(errno));
        return JIM_ERR;
    }
#else
    Jim_SetResultString(interp, "Not implemented", -1);
    return JIM_ERR;
#endif
}

static int file_cmd_join(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;
    char *newname = Jim_Alloc(MAXPATHLEN + 1);
    char *last = newname;

    *newname = 0;


    for (i = 0; i < argc; i++) {
        int len;
        const char *part = Jim_GetString(argv[i], &len);

        if (*part == '/') {

            last = newname;
        }
        else if (ISWINDOWS && strchr(part, ':')) {

            last = newname;
        }
        else if (part[0] == '.') {
            if (part[1] == '/') {
                part += 2;
                len -= 2;
            }
            else if (part[1] == 0 && last != newname) {

                continue;
            }
        }


        if (last != newname && last[-1] != '/') {
            *last++ = '/';
        }

        if (len) {
            if (last + len - newname >= MAXPATHLEN) {
                Jim_Free(newname);
                Jim_SetResultString(interp, "Path too long", -1);
                return JIM_ERR;
            }
            memcpy(last, part, len);
            last += len;
        }


        if (last > newname + 1 && last[-1] == '/') {

            if (!ISWINDOWS || !(last > newname + 2 && last[-2] == ':')) {
                *--last = 0;
            }
        }
    }

    *last = 0;



    Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, newname, last - newname));

    return JIM_OK;
}

static int file_access(Jim_Interp *interp, Jim_Obj *filename, int mode)
{
    Jim_SetResultBool(interp, access(Jim_String(filename), mode) != -1);

    return JIM_OK;
}

static int file_cmd_readable(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return file_access(interp, argv[0], R_OK);
}

static int file_cmd_writable(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return file_access(interp, argv[0], W_OK);
}

static int file_cmd_executable(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
#ifdef X_OK
    return file_access(interp, argv[0], X_OK);
#else

    Jim_SetResultBool(interp, 1);
    return JIM_OK;
#endif
}

static int file_cmd_exists(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return file_access(interp, argv[0], F_OK);
}

static int file_cmd_delete(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int force = Jim_CompareStringImmediate(interp, argv[0], "-force");

    if (force || Jim_CompareStringImmediate(interp, argv[0], "--")) {
        argc++;
        argv--;
    }

    while (argc--) {
        const char *path = Jim_String(argv[0]);

        if (unlink(path) == -1 && errno != ENOENT) {
            if (rmdir(path) == -1) {

                if (!force || Jim_EvalPrefix(interp, "file delete force", 1, argv) != JIM_OK) {
                    Jim_SetResultFormatted(interp, "couldn't delete file \"%s\": %s", path,
                        strerror(errno));
                    return JIM_ERR;
                }
            }
        }
        argv++;
    }
    return JIM_OK;
}

#ifdef HAVE_MKDIR_ONE_ARG
#define MKDIR_DEFAULT(PATHNAME) mkdir(PATHNAME)
#else
#define MKDIR_DEFAULT(PATHNAME) mkdir(PATHNAME, 0755)
#endif

static int mkdir_all(char *path)
{
    int ok = 1;


    goto first;

    while (ok--) {

        {
            char *slash = strrchr(path, '/');

            if (slash && slash != path) {
                *slash = 0;
                if (mkdir_all(path) != 0) {
                    return -1;
                }
                *slash = '/';
            }
        }
      first:
        if (MKDIR_DEFAULT(path) == 0) {
            return 0;
        }
        if (errno == ENOENT) {

            continue;
        }

        if (errno == EEXIST) {
            struct stat sb;

            if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
                return 0;
            }

            errno = EEXIST;
        }

        break;
    }
    return -1;
}

static int file_cmd_mkdir(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    while (argc--) {
        char *path = Jim_StrDup(Jim_String(argv[0]));
        int rc = mkdir_all(path);

        Jim_Free(path);
        if (rc != 0) {
            Jim_SetResultFormatted(interp, "can't create directory \"%#s\": %s", argv[0],
                strerror(errno));
            return JIM_ERR;
        }
        argv++;
    }
    return JIM_OK;
}

static int file_cmd_tempfile(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int fd = Jim_MakeTempFile(interp, (argc >= 1) ? Jim_String(argv[0]) : NULL, 0);

    if (fd < 0) {
        return JIM_ERR;
    }
    close(fd);

    return JIM_OK;
}

static int file_cmd_rename(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *source;
    const char *dest;
    int force = 0;

    if (argc == 3) {
        if (!Jim_CompareStringImmediate(interp, argv[0], "-force")) {
            return -1;
        }
        force++;
        argv++;
        argc--;
    }

    source = Jim_String(argv[0]);
    dest = Jim_String(argv[1]);

    if (!force && access(dest, F_OK) == 0) {
        Jim_SetResultFormatted(interp, "error renaming \"%#s\" to \"%#s\": target exists", argv[0],
            argv[1]);
        return JIM_ERR;
    }

    if (rename(source, dest) != 0) {
        Jim_SetResultFormatted(interp, "error renaming \"%#s\" to \"%#s\": %s", argv[0], argv[1],
            strerror(errno));
        return JIM_ERR;
    }

    return JIM_OK;
}

#if defined(HAVE_LINK) && defined(HAVE_SYMLINK)
static int file_cmd_link(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int ret;
    const char *source;
    const char *dest;
    static const char * const options[] = { "-hard", "-symbolic", NULL };
    enum { OPT_HARD, OPT_SYMBOLIC, };
    int option = OPT_HARD;

    if (argc == 3) {
        if (Jim_GetEnum(interp, argv[0], options, &option, NULL, JIM_ENUM_ABBREV | JIM_ERRMSG) != JIM_OK) {
            return JIM_ERR;
        }
        argv++;
        argc--;
    }

    dest = Jim_String(argv[0]);
    source = Jim_String(argv[1]);

    if (option == OPT_HARD) {
        ret = link(source, dest);
    }
    else {
        ret = symlink(source, dest);
    }

    if (ret != 0) {
        Jim_SetResultFormatted(interp, "error linking \"%#s\" to \"%#s\": %s", argv[0], argv[1],
            strerror(errno));
        return JIM_ERR;
    }

    return JIM_OK;
}
#endif

static int file_stat(Jim_Interp *interp, Jim_Obj *filename, struct stat *sb)
{
    const char *path = Jim_String(filename);

    if (stat(path, sb) == -1) {
        Jim_SetResultFormatted(interp, "could not read \"%#s\": %s", filename, strerror(errno));
        return JIM_ERR;
    }
    return JIM_OK;
}

#ifdef HAVE_LSTAT
static int file_lstat(Jim_Interp *interp, Jim_Obj *filename, struct stat *sb)
{
    const char *path = Jim_String(filename);

    if (lstat(path, sb) == -1) {
        Jim_SetResultFormatted(interp, "could not read \"%#s\": %s", filename, strerror(errno));
        return JIM_ERR;
    }
    return JIM_OK;
}
#else
#define file_lstat file_stat
#endif

static int file_cmd_atime(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;

    if (file_stat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, sb.st_atime);
    return JIM_OK;
}

static int JimSetFileTimes(Jim_Interp *interp, const char *filename, jim_wide us)
{
#ifdef HAVE_UTIMES
    struct timeval times[2];

    times[1].tv_sec = times[0].tv_sec = us / 1000000;
    times[1].tv_usec = times[0].tv_usec = us % 1000000;

    if (utimes(filename, times) != 0) {
        Jim_SetResultFormatted(interp, "can't set time on \"%s\": %s", filename, strerror(errno));
        return JIM_ERR;
    }
    return JIM_OK;
#else
    Jim_SetResultString(interp, "Not implemented", -1);
    return JIM_ERR;
#endif
}

static int file_cmd_mtime(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;

    if (argc == 2) {
        jim_wide secs;
        if (Jim_GetWide(interp, argv[1], &secs) != JIM_OK) {
            return JIM_ERR;
        }
        return JimSetFileTimes(interp, Jim_String(argv[0]), secs * 1000000);
    }
    if (file_stat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, sb.st_mtime);
    return JIM_OK;
}

#ifdef STAT_MTIME_US
static int file_cmd_mtimeus(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;

    if (argc == 2) {
        jim_wide us;
        if (Jim_GetWide(interp, argv[1], &us) != JIM_OK) {
            return JIM_ERR;
        }
        return JimSetFileTimes(interp, Jim_String(argv[0]), us);
    }
    if (file_stat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, STAT_MTIME_US(sb));
    return JIM_OK;
}
#endif

static int file_cmd_copy(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_EvalPrefix(interp, "file copy", argc, argv);
}

static int file_cmd_size(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;

    if (file_stat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, sb.st_size);
    return JIM_OK;
}

static int file_cmd_isdirectory(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;
    int ret = 0;

    if (file_stat(interp, argv[0], &sb) == JIM_OK) {
        ret = S_ISDIR(sb.st_mode);
    }
    Jim_SetResultInt(interp, ret);
    return JIM_OK;
}

static int file_cmd_isfile(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;
    int ret = 0;

    if (file_stat(interp, argv[0], &sb) == JIM_OK) {
        ret = S_ISREG(sb.st_mode);
    }
    Jim_SetResultInt(interp, ret);
    return JIM_OK;
}

#ifdef HAVE_GETEUID
static int file_cmd_owned(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;
    int ret = 0;

    if (file_stat(interp, argv[0], &sb) == JIM_OK) {
        ret = (geteuid() == sb.st_uid);
    }
    Jim_SetResultInt(interp, ret);
    return JIM_OK;
}
#endif

#if defined(HAVE_READLINK)
static int file_cmd_readlink(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *path = Jim_String(argv[0]);
    char *linkValue = Jim_Alloc(MAXPATHLEN + 1);

    int linkLength = readlink(path, linkValue, MAXPATHLEN);

    if (linkLength == -1) {
        Jim_Free(linkValue);
        Jim_SetResultFormatted(interp, "couldn't readlink \"%#s\": %s", argv[0], strerror(errno));
        return JIM_ERR;
    }
    linkValue[linkLength] = 0;
    Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, linkValue, linkLength));
    return JIM_OK;
}
#endif

static int file_cmd_type(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;

    if (file_lstat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResultString(interp, JimGetFileType((int)sb.st_mode), -1);
    return JIM_OK;
}

#ifdef HAVE_LSTAT
static int file_cmd_lstat(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;

    if (file_lstat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    return StoreStatData(interp, argc == 2 ? argv[1] : NULL, &sb);
}
#else
#define file_cmd_lstat file_cmd_stat
#endif

static int file_cmd_stat(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;

    if (file_stat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    return StoreStatData(interp, argc == 2 ? argv[1] : NULL, &sb);
}

static const jim_subcmd_type file_command_table[] = {
    {   "atime",
        "name",
        file_cmd_atime,
        1,
        1,

    },
    {   "mtime",
        "name ?time?",
        file_cmd_mtime,
        1,
        2,

    },
#ifdef STAT_MTIME_US
    {   "mtimeus",
        "name ?time?",
        file_cmd_mtimeus,
        1,
        2,

    },
#endif
    {   "copy",
        "?-force? source dest",
        file_cmd_copy,
        2,
        3,

    },
    {   "dirname",
        "name",
        file_cmd_dirname,
        1,
        1,

    },
    {   "rootname",
        "name",
        file_cmd_rootname,
        1,
        1,

    },
    {   "extension",
        "name",
        file_cmd_extension,
        1,
        1,

    },
    {   "tail",
        "name",
        file_cmd_tail,
        1,
        1,

    },
    {   "normalize",
        "name",
        file_cmd_normalize,
        1,
        1,

    },
    {   "join",
        "name ?name ...?",
        file_cmd_join,
        1,
        -1,

    },
    {   "readable",
        "name",
        file_cmd_readable,
        1,
        1,

    },
    {   "writable",
        "name",
        file_cmd_writable,
        1,
        1,

    },
    {   "executable",
        "name",
        file_cmd_executable,
        1,
        1,

    },
    {   "exists",
        "name",
        file_cmd_exists,
        1,
        1,

    },
    {   "delete",
        "?-force|--? name ...",
        file_cmd_delete,
        1,
        -1,

    },
    {   "mkdir",
        "dir ...",
        file_cmd_mkdir,
        1,
        -1,

    },
    {   "tempfile",
        "?template?",
        file_cmd_tempfile,
        0,
        1,

    },
    {   "rename",
        "?-force? source dest",
        file_cmd_rename,
        2,
        3,

    },
#if defined(HAVE_LINK) && defined(HAVE_SYMLINK)
    {   "link",
        "?-symbolic|-hard? newname target",
        file_cmd_link,
        2,
        3,

    },
#endif
#if defined(HAVE_READLINK)
    {   "readlink",
        "name",
        file_cmd_readlink,
        1,
        1,

    },
#endif
    {   "size",
        "name",
        file_cmd_size,
        1,
        1,

    },
    {   "stat",
        "name ?var?",
        file_cmd_stat,
        1,
        2,

    },
    {   "lstat",
        "name ?var?",
        file_cmd_lstat,
        1,
        2,

    },
    {   "type",
        "name",
        file_cmd_type,
        1,
        1,

    },
#ifdef HAVE_GETEUID
    {   "owned",
        "name",
        file_cmd_owned,
        1,
        1,

    },
#endif
    {   "isdirectory",
        "name",
        file_cmd_isdirectory,
        1,
        1,

    },
    {   "isfile",
        "name",
        file_cmd_isfile,
        1,
        1,

    },
    {
        NULL
    }
};

static int Jim_CdCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *path;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "dirname");
        return JIM_ERR;
    }

    path = Jim_String(argv[1]);

    if (chdir(path) != 0) {
        Jim_SetResultFormatted(interp, "couldn't change working directory to \"%s\": %s", path,
            strerror(errno));
        return JIM_ERR;
    }
    return JIM_OK;
}

static int Jim_PwdCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char *cwd = Jim_Alloc(MAXPATHLEN);

    if (getcwd(cwd, MAXPATHLEN) == NULL) {
        Jim_SetResultString(interp, "Failed to get pwd", -1);
        Jim_Free(cwd);
        return JIM_ERR;
    }
    else if (ISWINDOWS) {

        char *p = cwd;
        while ((p = strchr(p, '\\')) != NULL) {
            *p++ = '/';
        }
    }

    Jim_SetResultString(interp, cwd, -1);

    Jim_Free(cwd);
    return JIM_OK;
}

int Jim_fileInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "file", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "file", Jim_SubCmdProc, (void *)file_command_table, NULL);
    Jim_CreateCommand(interp, "pwd", Jim_PwdCmd, NULL, NULL);
    Jim_CreateCommand(interp, "cd", Jim_CdCmd, NULL, NULL);
    return JIM_OK;
}

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <string.h>
#include <ctype.h>


#if (!defined(HAVE_VFORK) || !defined(HAVE_WAITPID)) && !defined(__MINGW32__)
static int Jim_ExecCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *cmdlineObj = Jim_NewEmptyStringObj(interp);
    int i, j;
    int rc;


    for (i = 1; i < argc; i++) {
        int len;
        const char *arg = Jim_GetString(argv[i], &len);

        if (i > 1) {
            Jim_AppendString(interp, cmdlineObj, " ", 1);
        }
        if (strpbrk(arg, "\\\" ") == NULL) {

            Jim_AppendString(interp, cmdlineObj, arg, len);
            continue;
        }

        Jim_AppendString(interp, cmdlineObj, "\"", 1);
        for (j = 0; j < len; j++) {
            if (arg[j] == '\\' || arg[j] == '"') {
                Jim_AppendString(interp, cmdlineObj, "\\", 1);
            }
            Jim_AppendString(interp, cmdlineObj, &arg[j], 1);
        }
        Jim_AppendString(interp, cmdlineObj, "\"", 1);
    }
    rc = system(Jim_String(cmdlineObj));

    Jim_FreeNewObj(interp, cmdlineObj);

    if (rc) {
        Jim_Obj *errorCode = Jim_NewListObj(interp, NULL, 0);
        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, "CHILDSTATUS", -1));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, 0));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, rc));
        Jim_SetGlobalVariableStr(interp, "errorCode", errorCode);
        return JIM_ERR;
    }

    return JIM_OK;
}

int Jim_execInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "exec", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "exec", Jim_ExecCmd, NULL, NULL);
    return JIM_OK;
}
#else


#include <errno.h>
#include <signal.h>
#include <sys/stat.h>

struct WaitInfoTable;

static char **JimOriginalEnviron(void);
static char **JimSaveEnv(char **env);
static void JimRestoreEnv(char **env);
static int JimCreatePipeline(Jim_Interp *interp, int argc, Jim_Obj *const *argv,
    pidtype **pidArrayPtr, int *inPipePtr, int *outPipePtr, int *errFilePtr);
static void JimDetachPids(struct WaitInfoTable *table, int numPids, const pidtype *pidPtr);
static int JimCleanupChildren(Jim_Interp *interp, int numPids, pidtype *pidPtr, Jim_Obj *errStrObj);
static int Jim_WaitCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv);

#if defined(__MINGW32__)
static pidtype JimStartWinProcess(Jim_Interp *interp, char **argv, char **env, int inputId, int outputId, int errorId);
#endif

static void Jim_RemoveTrailingNewline(Jim_Obj *objPtr)
{
    int len;
    const char *s = Jim_GetString(objPtr, &len);

    if (len > 0 && s[len - 1] == '\n') {
        objPtr->length--;
        objPtr->bytes[objPtr->length] = '\0';
    }
}

static int JimAppendStreamToString(Jim_Interp *interp, int fd, Jim_Obj *strObj)
{
    char buf[256];
    FILE *fh = fdopen(fd, "r");
    int ret = 0;

    if (fh == NULL) {
        return -1;
    }

    while (1) {
        int retval = fread(buf, 1, sizeof(buf), fh);
        if (retval > 0) {
            ret = 1;
            Jim_AppendString(interp, strObj, buf, retval);
        }
        if (retval != sizeof(buf)) {
            break;
        }
    }
    fclose(fh);
    return ret;
}

static char **JimBuildEnv(Jim_Interp *interp)
{
    int i;
    int size;
    int num;
    int n;
    char **envptr;
    char *envdata;

    Jim_Obj *objPtr = Jim_GetGlobalVariableStr(interp, "env", JIM_NONE);

    if (!objPtr) {
        return JimOriginalEnviron();
    }



    num = Jim_ListLength(interp, objPtr);
    if (num % 2) {

        num--;
    }
    size = Jim_Length(objPtr) + 2;

    envptr = Jim_Alloc(sizeof(*envptr) * (num / 2 + 1) + size);
    envdata = (char *)&envptr[num / 2 + 1];

    n = 0;
    for (i = 0; i < num; i += 2) {
        const char *s1, *s2;
        Jim_Obj *elemObj;

        Jim_ListIndex(interp, objPtr, i, &elemObj, JIM_NONE);
        s1 = Jim_String(elemObj);
        Jim_ListIndex(interp, objPtr, i + 1, &elemObj, JIM_NONE);
        s2 = Jim_String(elemObj);

        envptr[n] = envdata;
        envdata += sprintf(envdata, "%s=%s", s1, s2);
        envdata++;
        n++;
    }
    envptr[n] = NULL;
    *envdata = 0;

    return envptr;
}

static void JimFreeEnv(char **env, char **original_environ)
{
    if (env != original_environ) {
        Jim_Free(env);
    }
}

static Jim_Obj *JimMakeErrorCode(Jim_Interp *interp, pidtype pid, int waitStatus, Jim_Obj *errStrObj)
{
    Jim_Obj *errorCode = Jim_NewListObj(interp, NULL, 0);

    if (pid == JIM_BAD_PID || pid == JIM_NO_PID) {
        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, "NONE", -1));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, (long)pid));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, -1));
    }
    else if (WIFEXITED(waitStatus)) {
        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, "CHILDSTATUS", -1));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, (long)pid));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, WEXITSTATUS(waitStatus)));
    }
    else {
        const char *type;
        const char *action;
        const char *signame;

        if (WIFSIGNALED(waitStatus)) {
            type = "CHILDKILLED";
            action = "killed";
            signame = Jim_SignalId(WTERMSIG(waitStatus));
        }
        else {
            type = "CHILDSUSP";
            action = "suspended";
            signame = "none";
        }

        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, type, -1));

        if (errStrObj) {
            Jim_AppendStrings(interp, errStrObj, "child ", action, " by signal ", Jim_SignalId(WTERMSIG(waitStatus)), "\n", NULL);
        }

        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, (long)pid));
        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, signame, -1));
    }
    return errorCode;
}

static int JimCheckWaitStatus(Jim_Interp *interp, pidtype pid, int waitStatus, Jim_Obj *errStrObj)
{
    if (WIFEXITED(waitStatus) && WEXITSTATUS(waitStatus) == 0) {
        return JIM_OK;
    }
    Jim_SetGlobalVariableStr(interp, "errorCode", JimMakeErrorCode(interp, pid, waitStatus, errStrObj));

    return JIM_ERR;
}


struct WaitInfo
{
    pidtype pid;
    int status;
    int flags;
};


struct WaitInfoTable {
    struct WaitInfo *info;
    int size;
    int used;
    int refcount;
};


#define WI_DETACHED 2

#define WAIT_TABLE_GROW_BY 4

static void JimFreeWaitInfoTable(struct Jim_Interp *interp, void *privData)
{
    struct WaitInfoTable *table = privData;

    if (--table->refcount == 0) {
        Jim_Free(table->info);
        Jim_Free(table);
    }
}

static struct WaitInfoTable *JimAllocWaitInfoTable(void)
{
    struct WaitInfoTable *table = Jim_Alloc(sizeof(*table));
    table->info = NULL;
    table->size = table->used = 0;
    table->refcount = 1;

    return table;
}

static int JimWaitRemove(struct WaitInfoTable *table, pidtype pid)
{
    int i;


    for (i = 0; i < table->used; i++) {
        if (pid == table->info[i].pid) {
            if (i != table->used - 1) {
                table->info[i] = table->info[table->used - 1];
            }
            table->used--;
            return 0;
        }
    }
    return -1;
}

static int Jim_ExecCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int outputId;
    int errorId;
    pidtype *pidPtr;
    int numPids, result;
    int child_siginfo = 1;
    Jim_Obj *childErrObj;
    Jim_Obj *errStrObj;
    struct WaitInfoTable *table = Jim_CmdPrivData(interp);

    if (argc > 1 && Jim_CompareStringImmediate(interp, argv[argc - 1], "&")) {
        Jim_Obj *listObj;
        int i;

        argc--;
        numPids = JimCreatePipeline(interp, argc - 1, argv + 1, &pidPtr, NULL, NULL, NULL);
        if (numPids < 0) {
            return JIM_ERR;
        }

        listObj = Jim_NewListObj(interp, NULL, 0);
        for (i = 0; i < numPids; i++) {
            Jim_ListAppendElement(interp, listObj, Jim_NewIntObj(interp, (long)pidPtr[i]));
        }
        Jim_SetResult(interp, listObj);
        JimDetachPids(table, numPids, pidPtr);
        Jim_Free(pidPtr);
        return JIM_OK;
    }

    numPids =
        JimCreatePipeline(interp, argc - 1, argv + 1, &pidPtr, NULL, &outputId, &errorId);

    if (numPids < 0) {
        return JIM_ERR;
    }

    result = JIM_OK;

    errStrObj = Jim_NewStringObj(interp, "", 0);


    if (outputId != -1) {
        if (JimAppendStreamToString(interp, outputId, errStrObj) < 0) {
            result = JIM_ERR;
            Jim_SetResultErrno(interp, "error reading from output pipe");
        }
    }


    childErrObj = Jim_NewStringObj(interp, "", 0);
    Jim_IncrRefCount(childErrObj);

    if (JimCleanupChildren(interp, numPids, pidPtr, childErrObj) != JIM_OK) {
        result = JIM_ERR;
    }

    if (errorId != -1) {
        int ret;
        lseek(errorId, 0, SEEK_SET);
        ret = JimAppendStreamToString(interp, errorId, errStrObj);
        if (ret < 0) {
            Jim_SetResultErrno(interp, "error reading from error pipe");
            result = JIM_ERR;
        }
        else if (ret > 0) {

            child_siginfo = 0;
        }
    }

    if (child_siginfo) {

        Jim_AppendObj(interp, errStrObj, childErrObj);
    }
    Jim_DecrRefCount(interp, childErrObj);


    Jim_RemoveTrailingNewline(errStrObj);


    Jim_SetResult(interp, errStrObj);

    return result;
}

static pidtype JimWaitForProcess(struct WaitInfoTable *table, pidtype pid, int *statusPtr)
{
    if (JimWaitRemove(table, pid) == 0) {

         waitpid(pid, statusPtr, 0);
         return pid;
    }


    return JIM_BAD_PID;
}

static void JimDetachPids(struct WaitInfoTable *table, int numPids, const pidtype *pidPtr)
{
    int j;

    for (j = 0; j < numPids; j++) {

        int i;
        for (i = 0; i < table->used; i++) {
            if (pidPtr[j] == table->info[i].pid) {
                table->info[i].flags |= WI_DETACHED;
                break;
            }
        }
    }
}

static int JimGetChannelFd(Jim_Interp *interp, const char *name)
{
    Jim_Obj *objv[2];

    objv[0] = Jim_NewStringObj(interp, name, -1);
    objv[1] = Jim_NewStringObj(interp, "getfd", -1);

    if (Jim_EvalObjVector(interp, 2, objv) == JIM_OK) {
        jim_wide fd;
        if (Jim_GetWide(interp, Jim_GetResult(interp), &fd) == JIM_OK) {
            return fd;
        }
    }
    return -1;
}

static void JimReapDetachedPids(struct WaitInfoTable *table)
{
    struct WaitInfo *waitPtr;
    int count;
    int dest;

    if (!table) {
        return;
    }

    waitPtr = table->info;
    dest = 0;
    for (count = table->used; count > 0; waitPtr++, count--) {
        if (waitPtr->flags & WI_DETACHED) {
            int status;
            pidtype pid = waitpid(waitPtr->pid, &status, WNOHANG);
            if (pid == waitPtr->pid) {

                table->used--;
                continue;
            }
        }
        if (waitPtr != &table->info[dest]) {
            table->info[dest] = *waitPtr;
        }
        dest++;
    }
}

static int Jim_WaitCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct WaitInfoTable *table = Jim_CmdPrivData(interp);
    int nohang = 0;
    pidtype pid;
    long pidarg;
    int status;
    Jim_Obj *errCodeObj;


    if (argc == 1) {
        JimReapDetachedPids(table);
        return JIM_OK;
    }

    if (argc > 1 && Jim_CompareStringImmediate(interp, argv[1], "-nohang")) {
        nohang = 1;
    }
    if (argc != nohang + 2) {
        Jim_WrongNumArgs(interp, 1, argv, "?-nohang? ?pid?");
        return JIM_ERR;
    }
    if (Jim_GetLong(interp, argv[nohang + 1], &pidarg) != JIM_OK) {
        return JIM_ERR;
    }

    pid = waitpid((pidtype)pidarg, &status, nohang ? WNOHANG : 0);

    errCodeObj = JimMakeErrorCode(interp, pid, status, NULL);

    if (pid != JIM_BAD_PID && (WIFEXITED(status) || WIFSIGNALED(status))) {

        JimWaitRemove(table, pid);
    }
    Jim_SetResult(interp, errCodeObj);
    return JIM_OK;
}

static int Jim_PidCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }

    Jim_SetResultInt(interp, (jim_wide)getpid());
    return JIM_OK;
}

static int
JimCreatePipeline(Jim_Interp *interp, int argc, Jim_Obj *const *argv, pidtype **pidArrayPtr,
    int *inPipePtr, int *outPipePtr, int *errFilePtr)
{
    pidtype *pidPtr = NULL;         /* Points to malloc-ed array holding all
                                 * the pids of child processes. */
    int numPids = 0;            /* Actual number of processes that exist
                                 * at *pidPtr right now. */
    int cmdCount;               /* Count of number of distinct commands
                                 * found in argc/argv. */
    const char *input = NULL;   /* Describes input for pipeline, depending
                                 * on "inputFile".  NULL means take input
                                 * from stdin/pipe. */
    int input_len = 0;

#define FILE_NAME   0
#define FILE_APPEND 1
#define FILE_HANDLE 2
#define FILE_TEXT   3

    int inputFile = FILE_NAME;  /* 1 means input is name of input file.
                                 * 2 means input is filehandle name.
                                 * 0 means input holds actual
                                 * text to be input to command. */

    int outputFile = FILE_NAME; /* 0 means output is the name of output file.
                                 * 1 means output is the name of output file, and append.
                                 * 2 means output is filehandle name.
                                 * All this is ignored if output is NULL
                                 */
    int errorFile = FILE_NAME;  /* 0 means error is the name of error file.
                                 * 1 means error is the name of error file, and append.
                                 * 2 means error is filehandle name.
                                 * All this is ignored if error is NULL
                                 */
    const char *output = NULL;  /* Holds name of output file to pipe to,
                                 * or NULL if output goes to stdout/pipe. */
    const char *error = NULL;   /* Holds name of stderr file to pipe to,
                                 * or NULL if stderr goes to stderr/pipe. */
    int inputId = -1;
    int outputId = -1;
    int errorId = -1;
    int lastOutputId = -1;
    int pipeIds[2];
    int firstArg, lastArg;      /* Indexes of first and last arguments in
                                 * current command. */
    int lastBar;
    int i;
    pidtype pid;
    char **save_environ;
#ifndef __MINGW32__
    char **child_environ;
#endif
    struct WaitInfoTable *table = Jim_CmdPrivData(interp);


    char **arg_array = Jim_Alloc(sizeof(*arg_array) * (argc + 1));
    int arg_count = 0;

    if (inPipePtr != NULL) {
        *inPipePtr = -1;
    }
    if (outPipePtr != NULL) {
        *outPipePtr = -1;
    }
    if (errFilePtr != NULL) {
        *errFilePtr = -1;
    }
    pipeIds[0] = pipeIds[1] = -1;

    cmdCount = 1;
    lastBar = -1;
    for (i = 0; i < argc; i++) {
        const char *arg = Jim_String(argv[i]);

        if (arg[0] == '<') {
            inputFile = FILE_NAME;
            input = arg + 1;
            if (*input == '<') {
                inputFile = FILE_TEXT;
                input_len = Jim_Length(argv[i]) - 2;
                input++;
            }
            else if (*input == '@') {
                inputFile = FILE_HANDLE;
                input++;
            }

            if (!*input && ++i < argc) {
                input = Jim_GetString(argv[i], &input_len);
            }
        }
        else if (arg[0] == '>') {
            int dup_error = 0;

            outputFile = FILE_NAME;

            output = arg + 1;
            if (*output == '>') {
                outputFile = FILE_APPEND;
                output++;
            }
            if (*output == '&') {

                output++;
                dup_error = 1;
            }
            if (*output == '@') {
                outputFile = FILE_HANDLE;
                output++;
            }
            if (!*output && ++i < argc) {
                output = Jim_String(argv[i]);
            }
            if (dup_error) {
                errorFile = outputFile;
                error = output;
            }
        }
        else if (arg[0] == '2' && arg[1] == '>') {
            error = arg + 2;
            errorFile = FILE_NAME;

            if (*error == '@') {
                errorFile = FILE_HANDLE;
                error++;
            }
            else if (*error == '>') {
                errorFile = FILE_APPEND;
                error++;
            }
            if (!*error && ++i < argc) {
                error = Jim_String(argv[i]);
            }
        }
        else {
            if (strcmp(arg, "|") == 0 || strcmp(arg, "|&") == 0) {
                if (i == lastBar + 1 || i == argc - 1) {
                    Jim_SetResultString(interp, "illegal use of | or |& in command", -1);
                    goto badargs;
                }
                lastBar = i;
                cmdCount++;
            }

            arg_array[arg_count++] = (char *)arg;
            continue;
        }

        if (i >= argc) {
            Jim_SetResultFormatted(interp, "can't specify \"%s\" as last word in command", arg);
            goto badargs;
        }
    }

    if (arg_count == 0) {
        Jim_SetResultString(interp, "didn't specify command to execute", -1);
badargs:
        Jim_Free(arg_array);
        return -1;
    }


    save_environ = JimSaveEnv(JimBuildEnv(interp));

    if (input != NULL) {
        if (inputFile == FILE_TEXT) {
            inputId = Jim_MakeTempFile(interp, NULL, 1);
            if (inputId == -1) {
                goto error;
            }
            if (write(inputId, input, input_len) != input_len) {
                Jim_SetResultErrno(interp, "couldn't write temp file");
                close(inputId);
                goto error;
            }
            lseek(inputId, 0L, SEEK_SET);
        }
        else if (inputFile == FILE_HANDLE) {
            int fd = JimGetChannelFd(interp, input);

            if (fd < 0) {
                goto error;
            }
            inputId = dup(fd);
        }
        else {
            inputId = Jim_OpenForRead(input);
            if (inputId == -1) {
                Jim_SetResultFormatted(interp, "couldn't read file \"%s\": %s", input, strerror(Jim_Errno()));
                goto error;
            }
        }
    }
    else if (inPipePtr != NULL) {
        if (pipe(pipeIds) != 0) {
            Jim_SetResultErrno(interp, "couldn't create input pipe for command");
            goto error;
        }
        inputId = pipeIds[0];
        *inPipePtr = pipeIds[1];
        pipeIds[0] = pipeIds[1] = -1;
    }

    if (output != NULL) {
        if (outputFile == FILE_HANDLE) {
            int fd = JimGetChannelFd(interp, output);
            if (fd < 0) {
                goto error;
            }
            lastOutputId = dup(fd);
        }
        else {
            lastOutputId = Jim_OpenForWrite(output, outputFile == FILE_APPEND);
            if (lastOutputId == -1) {
                Jim_SetResultFormatted(interp, "couldn't write file \"%s\": %s", output, strerror(Jim_Errno()));
                goto error;
            }
        }
    }
    else if (outPipePtr != NULL) {
        if (pipe(pipeIds) != 0) {
            Jim_SetResultErrno(interp, "couldn't create output pipe");
            goto error;
        }
        lastOutputId = pipeIds[1];
        *outPipePtr = pipeIds[0];
        pipeIds[0] = pipeIds[1] = -1;
    }

    if (error != NULL) {
        if (errorFile == FILE_HANDLE) {
            if (strcmp(error, "1") == 0) {

                if (lastOutputId != -1) {
                    errorId = dup(lastOutputId);
                }
                else {

                    error = "stdout";
                }
            }
            if (errorId == -1) {
                int fd = JimGetChannelFd(interp, error);
                if (fd < 0) {
                    goto error;
                }
                errorId = dup(fd);
            }
        }
        else {
            errorId = Jim_OpenForWrite(error, errorFile == FILE_APPEND);
            if (errorId == -1) {
                Jim_SetResultFormatted(interp, "couldn't write file \"%s\": %s", error, strerror(Jim_Errno()));
                goto error;
            }
        }
    }
    else if (errFilePtr != NULL) {
        errorId = Jim_MakeTempFile(interp, NULL, 1);
        if (errorId == -1) {
            goto error;
        }
        *errFilePtr = dup(errorId);
    }


    pidPtr = Jim_Alloc(cmdCount * sizeof(*pidPtr));
    for (i = 0; i < numPids; i++) {
        pidPtr[i] = JIM_BAD_PID;
    }
    for (firstArg = 0; firstArg < arg_count; numPids++, firstArg = lastArg + 1) {
        int pipe_dup_err = 0;
        int origErrorId = errorId;

        for (lastArg = firstArg; lastArg < arg_count; lastArg++) {
            if (strcmp(arg_array[lastArg], "|") == 0) {
                break;
            }
            if (strcmp(arg_array[lastArg], "|&") == 0) {
                pipe_dup_err = 1;
                break;
            }
        }

        if (lastArg == firstArg) {
            Jim_SetResultString(interp, "missing command to exec", -1);
            goto error;
        }


        arg_array[lastArg] = NULL;
        if (lastArg == arg_count) {
            outputId = lastOutputId;
            lastOutputId = -1;
        }
        else {
            if (pipe(pipeIds) != 0) {
                Jim_SetResultErrno(interp, "couldn't create pipe");
                goto error;
            }
            outputId = pipeIds[1];
        }


        if (pipe_dup_err) {
            errorId = outputId;
        }



#ifdef __MINGW32__
        pid = JimStartWinProcess(interp, &arg_array[firstArg], save_environ, inputId, outputId, errorId);
        if (pid == JIM_BAD_PID) {
            Jim_SetResultFormatted(interp, "couldn't exec \"%s\"", arg_array[firstArg]);
            goto error;
        }
#else
        i = strlen(arg_array[firstArg]);

        child_environ = Jim_GetEnviron();
        pid = vfork();
        if (pid < 0) {
            Jim_SetResultErrno(interp, "couldn't fork child process");
            goto error;
        }
        if (pid == 0) {


            if (inputId != -1) {
                dup2(inputId, fileno(stdin));
                close(inputId);
            }
            if (outputId != -1) {
                dup2(outputId, fileno(stdout));
                if (outputId != errorId) {
                    close(outputId);
                }
            }
            if (errorId != -1) {
                dup2(errorId, fileno(stderr));
                close(errorId);
            }

            if (outPipePtr) {
                close(*outPipePtr);
            }
            if (errFilePtr) {
                close(*errFilePtr);
            }
            if (pipeIds[0] != -1) {
                close(pipeIds[0]);
            }
            if (lastOutputId != -1) {
                close(lastOutputId);
            }


            (void)signal(SIGPIPE, SIG_DFL);

            execvpe(arg_array[firstArg], &arg_array[firstArg], child_environ);

            if (write(fileno(stderr), "couldn't exec \"", 15) &&
                write(fileno(stderr), arg_array[firstArg], i) &&
                write(fileno(stderr), "\"\n", 2)) {

            }
#ifdef JIM_MAINTAINER
            {

                static char *const false_argv[2] = {"false", NULL};
                execvp(false_argv[0],false_argv);
            }
#endif
            _exit(127);
        }
#endif



        if (table->used == table->size) {
            table->size += WAIT_TABLE_GROW_BY;
            table->info = Jim_Realloc(table->info, table->size * sizeof(*table->info));
        }

        table->info[table->used].pid = pid;
        table->info[table->used].flags = 0;
        table->used++;

        pidPtr[numPids] = pid;


        errorId = origErrorId;


        if (inputId != -1) {
            close(inputId);
        }
        if (outputId != -1) {
            close(outputId);
        }
        inputId = pipeIds[0];
        pipeIds[0] = pipeIds[1] = -1;
    }
    *pidArrayPtr = pidPtr;


  cleanup:
    if (inputId != -1) {
        close(inputId);
    }
    if (lastOutputId != -1) {
        close(lastOutputId);
    }
    if (errorId != -1) {
        close(errorId);
    }
    Jim_Free(arg_array);

    JimRestoreEnv(save_environ);

    return numPids;


  error:
    if ((inPipePtr != NULL) && (*inPipePtr != -1)) {
        close(*inPipePtr);
        *inPipePtr = -1;
    }
    if ((outPipePtr != NULL) && (*outPipePtr != -1)) {
        close(*outPipePtr);
        *outPipePtr = -1;
    }
    if ((errFilePtr != NULL) && (*errFilePtr != -1)) {
        close(*errFilePtr);
        *errFilePtr = -1;
    }
    if (pipeIds[0] != -1) {
        close(pipeIds[0]);
    }
    if (pipeIds[1] != -1) {
        close(pipeIds[1]);
    }
    if (pidPtr != NULL) {
        for (i = 0; i < numPids; i++) {
            if (pidPtr[i] != JIM_BAD_PID) {
                JimDetachPids(table, 1, &pidPtr[i]);
            }
        }
        Jim_Free(pidPtr);
    }
    numPids = -1;
    goto cleanup;
}


static int JimCleanupChildren(Jim_Interp *interp, int numPids, pidtype *pidPtr, Jim_Obj *errStrObj)
{
    struct WaitInfoTable *table = Jim_CmdPrivData(interp);
    int result = JIM_OK;
    int i;


    for (i = 0; i < numPids; i++) {
        int waitStatus = 0;
        if (JimWaitForProcess(table, pidPtr[i], &waitStatus) != JIM_BAD_PID) {
            if (JimCheckWaitStatus(interp, pidPtr[i], waitStatus, errStrObj) != JIM_OK) {
                result = JIM_ERR;
            }
        }
    }
    Jim_Free(pidPtr);

    return result;
}

int Jim_execInit(Jim_Interp *interp)
{
    struct WaitInfoTable *waitinfo;
    if (Jim_PackageProvide(interp, "exec", "1.0", JIM_ERRMSG))
        return JIM_ERR;

#ifdef SIGPIPE
    (void)signal(SIGPIPE, SIG_IGN);
#endif

    waitinfo = JimAllocWaitInfoTable();
    Jim_CreateCommand(interp, "exec", Jim_ExecCmd, waitinfo, JimFreeWaitInfoTable);
    waitinfo->refcount++;
    Jim_CreateCommand(interp, "wait", Jim_WaitCommand, waitinfo, JimFreeWaitInfoTable);
    Jim_CreateCommand(interp, "pid", Jim_PidCommand, 0, 0);

    return JIM_OK;
}

#if defined(__MINGW32__)


static int
JimWinFindExecutable(const char *originalName, char fullPath[MAX_PATH])
{
    int i;
    static char extensions[][5] = {".exe", "", ".bat"};

    for (i = 0; i < (int) (sizeof(extensions) / sizeof(extensions[0])); i++) {
        snprintf(fullPath, MAX_PATH, "%s%s", originalName, extensions[i]);

        if (SearchPath(NULL, fullPath, NULL, MAX_PATH, fullPath, NULL) == 0) {
            continue;
        }
        if (GetFileAttributes(fullPath) & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        return 0;
    }

    return -1;
}

static char **JimSaveEnv(char **env)
{
    return env;
}

static void JimRestoreEnv(char **env)
{
    JimFreeEnv(env, Jim_GetEnviron());
}

static char **JimOriginalEnviron(void)
{
    return NULL;
}

static Jim_Obj *
JimWinBuildCommandLine(Jim_Interp *interp, char **argv)
{
    char *start, *special;
    int quote, i;

    Jim_Obj *strObj = Jim_NewStringObj(interp, "", 0);

    for (i = 0; argv[i]; i++) {
        if (i > 0) {
            Jim_AppendString(interp, strObj, " ", 1);
        }

        if (argv[i][0] == '\0') {
            quote = 1;
        }
        else {
            quote = 0;
            for (start = argv[i]; *start != '\0'; start++) {
                if (isspace(UCHAR(*start))) {
                    quote = 1;
                    break;
                }
            }
        }
        if (quote) {
            Jim_AppendString(interp, strObj, "\"" , 1);
        }

        start = argv[i];
        for (special = argv[i]; ; ) {
            if ((*special == '\\') && (special[1] == '\\' ||
                    special[1] == '"' || (quote && special[1] == '\0'))) {
                Jim_AppendString(interp, strObj, start, special - start);
                start = special;
                while (1) {
                    special++;
                    if (*special == '"' || (quote && *special == '\0')) {

                        Jim_AppendString(interp, strObj, start, special - start);
                        break;
                    }
                    if (*special != '\\') {
                        break;
                    }
                }
                Jim_AppendString(interp, strObj, start, special - start);
                start = special;
            }
            if (*special == '"') {
        if (special == start) {
            Jim_AppendString(interp, strObj, "\"", 1);
        }
        else {
            Jim_AppendString(interp, strObj, start, special - start);
        }
                Jim_AppendString(interp, strObj, "\\\"", 2);
                start = special + 1;
            }
            if (*special == '\0') {
                break;
            }
            special++;
        }
        Jim_AppendString(interp, strObj, start, special - start);
        if (quote) {
            Jim_AppendString(interp, strObj, "\"", 1);
        }
    }
    return strObj;
}

static pidtype
JimStartWinProcess(Jim_Interp *interp, char **argv, char **env, int inputId, int outputId, int errorId)
{
    STARTUPINFO startInfo;
    PROCESS_INFORMATION procInfo;
    HANDLE hProcess;
    char execPath[MAX_PATH];
    pidtype pid = JIM_BAD_PID;
    Jim_Obj *cmdLineObj;
    char *winenv;

    if (JimWinFindExecutable(argv[0], execPath) < 0) {
        return JIM_BAD_PID;
    }
    argv[0] = execPath;

    hProcess = GetCurrentProcess();
    cmdLineObj = JimWinBuildCommandLine(interp, argv);


    ZeroMemory(&startInfo, sizeof(startInfo));
    startInfo.cb = sizeof(startInfo);
    startInfo.dwFlags   = STARTF_USESTDHANDLES;
    startInfo.hStdInput = INVALID_HANDLE_VALUE;
    startInfo.hStdOutput= INVALID_HANDLE_VALUE;
    startInfo.hStdError = INVALID_HANDLE_VALUE;

    if (inputId == -1) {
        inputId = _fileno(stdin);
    }
    DuplicateHandle(hProcess, (HANDLE)_get_osfhandle(inputId), hProcess, &startInfo.hStdInput,
            0, TRUE, DUPLICATE_SAME_ACCESS);
    if (startInfo.hStdInput == INVALID_HANDLE_VALUE) {
        goto end;
    }

    if (outputId == -1) {
        outputId = _fileno(stdout);
    }
    DuplicateHandle(hProcess, (HANDLE)_get_osfhandle(outputId), hProcess, &startInfo.hStdOutput,
            0, TRUE, DUPLICATE_SAME_ACCESS);
    if (startInfo.hStdOutput == INVALID_HANDLE_VALUE) {
        goto end;
    }


    if (errorId == -1) {
        errorId = _fileno(stderr);
    }
    DuplicateHandle(hProcess, (HANDLE)_get_osfhandle(errorId), hProcess, &startInfo.hStdError,
            0, TRUE, DUPLICATE_SAME_ACCESS);
    if (startInfo.hStdError == INVALID_HANDLE_VALUE) {
        goto end;
    }

    if (env == NULL) {

        winenv = NULL;
    }
    else if (env[0] == NULL) {
        winenv = (char *)"\0";
    }
    else {
        winenv = env[0];
    }

    if (!CreateProcess(NULL, (char *)Jim_String(cmdLineObj), NULL, NULL, TRUE,
            0, winenv, NULL, &startInfo, &procInfo)) {
        goto end;
    }


    WaitForInputIdle(procInfo.hProcess, 5000);
    CloseHandle(procInfo.hThread);

    pid = procInfo.hProcess;

    end:
    Jim_FreeNewObj(interp, cmdLineObj);
    if (startInfo.hStdInput != INVALID_HANDLE_VALUE) {
        CloseHandle(startInfo.hStdInput);
    }
    if (startInfo.hStdOutput != INVALID_HANDLE_VALUE) {
        CloseHandle(startInfo.hStdOutput);
    }
    if (startInfo.hStdError != INVALID_HANDLE_VALUE) {
        CloseHandle(startInfo.hStdError);
    }
    return pid;
}

#else

static char **JimOriginalEnviron(void)
{
    return Jim_GetEnviron();
}

static char **JimSaveEnv(char **env)
{
    char **saveenv = Jim_GetEnviron();
    Jim_SetEnviron(env);
    return saveenv;
}

static void JimRestoreEnv(char **env)
{
    JimFreeEnv(Jim_GetEnviron(), env);
    Jim_SetEnviron(env);
}
#endif
#endif



#ifdef STRPTIME_NEEDS_XOPEN_SOURCE
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#endif


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>


#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

struct clock_options {
    int gmt;
    const char *format;
};

static int parse_clock_options(Jim_Interp *interp, int argc, Jim_Obj *const *argv, struct clock_options *opts)
{
    static const char * const options[] = { "-gmt", "-format", NULL };
    enum { OPT_GMT, OPT_FORMAT, };
    int i;

    for (i = 0; i < argc; i += 2) {
        int option;
        if (Jim_GetEnum(interp, argv[i], options, &option, NULL, JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
            return JIM_ERR;
        }
        switch (option) {
            case OPT_GMT:
                if (Jim_GetBoolean(interp, argv[i + 1], &opts->gmt) != JIM_OK) {
                    return JIM_ERR;
                }
                break;
            case OPT_FORMAT:
                opts->format = Jim_String(argv[i + 1]);
                break;
        }
    }
    return JIM_OK;
}

static int clock_cmd_format(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{

    char buf[100];
    time_t t;
    jim_wide seconds;
    struct clock_options options = { 0, "%a %b %d %H:%M:%S %Z %Y" };
    struct tm *tm;

    if (Jim_GetWide(interp, argv[0], &seconds) != JIM_OK) {
        return JIM_ERR;
    }
    if (argc % 2 == 0) {
        return -1;
    }
    if (parse_clock_options(interp, argc - 1, argv + 1, &options) == JIM_ERR) {
        return JIM_ERR;
    }

    t = seconds;
    tm = options.gmt ? gmtime(&t) : localtime(&t);

    if (tm == NULL || strftime(buf, sizeof(buf), options.format, tm) == 0) {
        Jim_SetResultString(interp, "format string too long or invalid time", -1);
        return JIM_ERR;
    }

    Jim_SetResultString(interp, buf, -1);

    return JIM_OK;
}

#ifdef HAVE_STRPTIME
static time_t jim_timegm(const struct tm *tm)
{
    int m = tm->tm_mon + 1;
    int y = 1900 + tm->tm_year - (m <= 2);
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + tm->tm_mday - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long days = (era * 146097 + (int)doe - 719468);
    int secs = tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;

    return days * 24 * 60 * 60 + secs;
}

static int clock_cmd_scan(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char *pt;
    struct tm tm;
    time_t now = time(NULL);

    struct clock_options options = { 0, NULL };

    if (argc % 2 == 0) {
        return -1;
    }

    if (parse_clock_options(interp, argc - 1, argv + 1, &options) == JIM_ERR) {
        return JIM_ERR;
    }
    if (options.format == NULL) {
        return -1;
    }

    localtime_r(&now, &tm);

    pt = strptime(Jim_String(argv[0]), options.format, &tm);
    if (pt == 0 || *pt != 0) {
        Jim_SetResultString(interp, "Failed to parse time according to format", -1);
        return JIM_ERR;
    }


    Jim_SetResultInt(interp, options.gmt ? jim_timegm(&tm) : mktime(&tm));

    return JIM_OK;
}
#endif

static int clock_cmd_seconds(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_SetResultInt(interp, time(NULL));

    return JIM_OK;
}

static int clock_cmd_micros(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    Jim_SetResultInt(interp, (jim_wide) tv.tv_sec * 1000000 + tv.tv_usec);

    return JIM_OK;
}

static int clock_cmd_millis(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    Jim_SetResultInt(interp, (jim_wide) tv.tv_sec * 1000 + tv.tv_usec / 1000);

    return JIM_OK;
}

static const jim_subcmd_type clock_command_table[] = {
    {   "clicks",
        NULL,
        clock_cmd_micros,
        0,
        0,

    },
    {   "format",
        "seconds ?-format string? ?-gmt boolean?",
        clock_cmd_format,
        1,
        5,

    },
    {   "microseconds",
        NULL,
        clock_cmd_micros,
        0,
        0,

    },
    {   "milliseconds",
        NULL,
        clock_cmd_millis,
        0,
        0,

    },
#ifdef HAVE_STRPTIME
    {   "scan",
        "str -format format ?-gmt boolean?",
        clock_cmd_scan,
        3,
        5,

    },
#endif
    {   "seconds",
        NULL,
        clock_cmd_seconds,
        0,
        0,

    },
    { NULL }
};

int Jim_clockInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "clock", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "clock", Jim_SubCmdProc, (void *)clock_command_table, NULL);
    return JIM_OK;
}

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>


static int array_cmd_exists(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{

    Jim_Obj *dictObj = Jim_GetVariable(interp, argv[0], JIM_UNSHARED);
    Jim_SetResultInt(interp, dictObj && Jim_DictSize(interp, dictObj) != -1);
    return JIM_OK;
}

static int array_cmd_get(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr = Jim_GetVariable(interp, argv[0], JIM_NONE);
    Jim_Obj *patternObj;

    if (!objPtr) {
        return JIM_OK;
    }

    patternObj = (argc == 1) ? NULL : argv[1];


    if (patternObj == NULL || Jim_CompareStringImmediate(interp, patternObj, "*")) {
        if (Jim_IsList(objPtr) && Jim_ListLength(interp, objPtr) % 2 == 0) {

            Jim_SetResult(interp, objPtr);
            return JIM_OK;
        }
    }

    return Jim_DictMatchTypes(interp, objPtr, patternObj, JIM_DICTMATCH_KEYS, JIM_DICTMATCH_KEYS | JIM_DICTMATCH_VALUES);
}

static int array_cmd_names(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr = Jim_GetVariable(interp, argv[0], JIM_NONE);

    if (!objPtr) {
        return JIM_OK;
    }

    return Jim_DictMatchTypes(interp, objPtr, argc == 1 ? NULL : argv[1], JIM_DICTMATCH_KEYS, JIM_DICTMATCH_KEYS);
}

static int array_cmd_unset(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;
    int len;
    Jim_Obj *resultObj;
    Jim_Obj *objPtr;
    Jim_Obj **dictValuesObj;

    if (argc == 1 || Jim_CompareStringImmediate(interp, argv[1], "*")) {

        Jim_UnsetVariable(interp, argv[0], JIM_NONE);
        return JIM_OK;
    }

    objPtr = Jim_GetVariable(interp, argv[0], JIM_NONE);

    if (objPtr == NULL) {

        return JIM_OK;
    }

    if (Jim_DictPairs(interp, objPtr, &dictValuesObj, &len) != JIM_OK) {

        Jim_SetResultString(interp, "", -1);
        return JIM_OK;
    }


    resultObj = Jim_NewDictObj(interp, NULL, 0);

    for (i = 0; i < len; i += 2) {
        if (!Jim_StringMatchObj(interp, argv[1], dictValuesObj[i], 0)) {
            Jim_DictAddElement(interp, resultObj, dictValuesObj[i], dictValuesObj[i + 1]);
        }
    }
    Jim_Free(dictValuesObj);

    Jim_SetVariable(interp, argv[0], resultObj);
    return JIM_OK;
}

static int array_cmd_size(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;
    int len = 0;


    objPtr = Jim_GetVariable(interp, argv[0], JIM_NONE);
    if (objPtr) {
        len = Jim_DictSize(interp, objPtr);
        if (len < 0) {

            Jim_SetResultInt(interp, 0);
            return JIM_OK;
        }
    }

    Jim_SetResultInt(interp, len);

    return JIM_OK;
}

static int array_cmd_stat(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr = Jim_GetVariable(interp, argv[0], JIM_NONE);
    if (objPtr) {
        return Jim_DictInfo(interp, objPtr);
    }
    Jim_SetResultFormatted(interp, "\"%#s\" isn't an array", argv[0], NULL);
    return JIM_ERR;
}

static int array_cmd_set(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;
    int len;
    Jim_Obj *listObj = argv[1];
    Jim_Obj *dictObj;

    len = Jim_ListLength(interp, listObj);
    if (len % 2) {
        Jim_SetResultString(interp, "list must have an even number of elements", -1);
        return JIM_ERR;
    }

    dictObj = Jim_GetVariable(interp, argv[0], JIM_UNSHARED);
    if (!dictObj) {

        return Jim_SetVariable(interp, argv[0], listObj);
    }
    else if (Jim_DictSize(interp, dictObj) < 0) {
        return JIM_ERR;
    }

    if (Jim_IsShared(dictObj)) {
        dictObj = Jim_DuplicateObj(interp, dictObj);
    }

    for (i = 0; i < len; i += 2) {
        Jim_Obj *nameObj;
        Jim_Obj *valueObj;

        Jim_ListIndex(interp, listObj, i, &nameObj, JIM_NONE);
        Jim_ListIndex(interp, listObj, i + 1, &valueObj, JIM_NONE);

        Jim_DictAddElement(interp, dictObj, nameObj, valueObj);
    }
    return Jim_SetVariable(interp, argv[0], dictObj);
}

static const jim_subcmd_type array_command_table[] = {
        {       "exists",
                "arrayName",
                array_cmd_exists,
                1,
                1,

        },
        {       "get",
                "arrayName ?pattern?",
                array_cmd_get,
                1,
                2,

        },
        {       "names",
                "arrayName ?pattern?",
                array_cmd_names,
                1,
                2,

        },
        {       "set",
                "arrayName list",
                array_cmd_set,
                2,
                2,

        },
        {       "size",
                "arrayName",
                array_cmd_size,
                1,
                1,

        },
        {       "stat",
                "arrayName",
                array_cmd_stat,
                1,
                1,

        },
        {       "unset",
                "arrayName ?pattern?",
                array_cmd_unset,
                1,
                2,

        },
        {       NULL
        }
};

int Jim_arrayInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "array", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "array", Jim_SubCmdProc, (void *)array_command_table, NULL);
    return JIM_OK;
}
int Jim_InitStaticExtensions(Jim_Interp *interp)
{
extern int Jim_bootstrapInit(Jim_Interp *);
extern int Jim_aioInit(Jim_Interp *);
extern int Jim_readdirInit(Jim_Interp *);
extern int Jim_regexpInit(Jim_Interp *);
extern int Jim_fileInit(Jim_Interp *);
extern int Jim_globInit(Jim_Interp *);
extern int Jim_execInit(Jim_Interp *);
extern int Jim_clockInit(Jim_Interp *);
extern int Jim_arrayInit(Jim_Interp *);
extern int Jim_stdlibInit(Jim_Interp *);
extern int Jim_tclcompatInit(Jim_Interp *);
Jim_bootstrapInit(interp);
Jim_aioInit(interp);
Jim_readdirInit(interp);
Jim_regexpInit(interp);
Jim_fileInit(interp);
Jim_globInit(interp);
Jim_execInit(interp);
Jim_clockInit(interp);
Jim_arrayInit(interp);
Jim_stdlibInit(interp);
Jim_tclcompatInit(interp);
return JIM_OK;
}
#define JIM_OPTIMIZATION
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <setjmp.h>


#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif
#ifdef HAVE_CRT_EXTERNS_H
#include <crt_externs.h>
#endif


#include <math.h>





#ifndef TCL_LIBRARY
#define TCL_LIBRARY "."
#endif
#ifndef TCL_PLATFORM_OS
#define TCL_PLATFORM_OS "unknown"
#endif
#ifndef TCL_PLATFORM_PLATFORM
#define TCL_PLATFORM_PLATFORM "unknown"
#endif
#ifndef TCL_PLATFORM_PATH_SEPARATOR
#define TCL_PLATFORM_PATH_SEPARATOR ":"
#endif







#ifdef JIM_MAINTAINER
#define JIM_DEBUG_COMMAND
#define JIM_DEBUG_PANIC
#endif



#define JIM_INTEGER_SPACE 24

const char *jim_tt_name(int type);

#ifdef JIM_DEBUG_PANIC
static void JimPanicDump(int fail_condition, const char *fmt, ...);
#define JimPanic(X) JimPanicDump X
#else
#define JimPanic(X)
#endif

#ifdef JIM_OPTIMIZATION
#define JIM_IF_OPTIM(X) X
#else
#define JIM_IF_OPTIM(X)
#endif


static char JimEmptyStringRep[] = "";

static void JimFreeCallFrame(Jim_Interp *interp, Jim_CallFrame *cf, int action);
static int ListSetIndex(Jim_Interp *interp, Jim_Obj *listPtr, int listindex, Jim_Obj *newObjPtr,
    int flags);
static int JimDeleteLocalProcs(Jim_Interp *interp, Jim_Stack *localCommands);
static Jim_Obj *JimExpandDictSugar(Jim_Interp *interp, Jim_Obj *objPtr);
static void SetDictSubstFromAny(Jim_Interp *interp, Jim_Obj *objPtr);
static Jim_Obj **JimDictPairs(Jim_Obj *dictPtr, int *len);
static void JimSetFailedEnumResult(Jim_Interp *interp, const char *arg, const char *badtype,
    const char *prefix, const char *const *tablePtr, const char *name);
static int JimCallProcedure(Jim_Interp *interp, Jim_Cmd *cmd, int argc, Jim_Obj *const *argv);
static int JimGetWideNoErr(Jim_Interp *interp, Jim_Obj *objPtr, jim_wide * widePtr);
static int JimSign(jim_wide w);
static int JimValidName(Jim_Interp *interp, const char *type, Jim_Obj *nameObjPtr);
static void JimPrngSeed(Jim_Interp *interp, unsigned char *seed, int seedLen);
static void JimRandomBytes(Jim_Interp *interp, void *dest, unsigned int len);



#define JimWideValue(objPtr) (objPtr)->internalRep.wideValue

#define JimObjTypeName(O) ((O)->typePtr ? (O)->typePtr->name : "none")

static int utf8_tounicode_case(const char *s, int *uc, int upper)
{
    int l = utf8_tounicode(s, uc);
    if (upper) {
        *uc = utf8_upper(*uc);
    }
    return l;
}


#define JIM_CHARSET_SCAN 2
#define JIM_CHARSET_GLOB 0

static const char *JimCharsetMatch(const char *pattern, int c, int flags)
{
    int not = 0;
    int pchar;
    int match = 0;
    int nocase = 0;

    if (flags & JIM_NOCASE) {
        nocase++;
        c = utf8_upper(c);
    }

    if (flags & JIM_CHARSET_SCAN) {
        if (*pattern == '^') {
            not++;
            pattern++;
        }


        if (*pattern == ']') {
            goto first;
        }
    }

    while (*pattern && *pattern != ']') {

        if (pattern[0] == '\\') {
first:
            pattern += utf8_tounicode_case(pattern, &pchar, nocase);
        }
        else {

            int start;
            int end;

            pattern += utf8_tounicode_case(pattern, &start, nocase);
            if (pattern[0] == '-' && pattern[1]) {

                pattern++;
                pattern += utf8_tounicode_case(pattern, &end, nocase);


                if ((c >= start && c <= end) || (c >= end && c <= start)) {
                    match = 1;
                }
                continue;
            }
            pchar = start;
        }

        if (pchar == c) {
            match = 1;
        }
    }
    if (not) {
        match = !match;
    }

    return match ? pattern : NULL;
}



static int JimGlobMatch(const char *pattern, const char *string, int nocase)
{
    int c;
    int pchar;
    while (*pattern) {
        switch (pattern[0]) {
            case '*':
                while (pattern[1] == '*') {
                    pattern++;
                }
                pattern++;
                if (!pattern[0]) {
                    return 1;
                }
                while (*string) {

                    if (JimGlobMatch(pattern, string, nocase))
                        return 1;
                    string += utf8_tounicode(string, &c);
                }
                return 0;

            case '?':
                string += utf8_tounicode(string, &c);
                break;

            case '[': {
                    string += utf8_tounicode(string, &c);
                    pattern = JimCharsetMatch(pattern + 1, c, nocase ? JIM_NOCASE : 0);
                    if (!pattern) {
                        return 0;
                    }
                    if (!*pattern) {

                        continue;
                    }
                    break;
                }
            case '\\':
                if (pattern[1]) {
                    pattern++;
                }

            default:
                string += utf8_tounicode_case(string, &c, nocase);
                utf8_tounicode_case(pattern, &pchar, nocase);
                if (pchar != c) {
                    return 0;
                }
                break;
        }
        pattern += utf8_tounicode_case(pattern, &pchar, nocase);
        if (!*string) {
            while (*pattern == '*') {
                pattern++;
            }
            break;
        }
    }
    if (!*pattern && !*string) {
        return 1;
    }
    return 0;
}

static int JimStringCompare(const char *s1, int l1, const char *s2, int l2)
{
    if (l1 < l2) {
        return memcmp(s1, s2, l1) <= 0 ? -1 : 1;
    }
    else if (l2 < l1) {
        return memcmp(s1, s2, l2) >= 0 ? 1 : -1;
    }
    else {
        return JimSign(memcmp(s1, s2, l1));
    }
}

static int JimStringCompareLen(const char *s1, const char *s2, int maxchars, int nocase)
{
    while (*s1 && *s2 && maxchars) {
        int c1, c2;
        s1 += utf8_tounicode_case(s1, &c1, nocase);
        s2 += utf8_tounicode_case(s2, &c2, nocase);
        if (c1 != c2) {
            return JimSign(c1 - c2);
        }
        maxchars--;
    }
    if (!maxchars) {
        return 0;
    }

    if (*s1) {
        return 1;
    }
    if (*s2) {
        return -1;
    }
    return 0;
}

static int JimStringFirst(const char *s1, int l1, const char *s2, int l2, int idx)
{
    int i;
    int l1bytelen;

    if (!l1 || !l2 || l1 > l2) {
        return -1;
    }
    if (idx < 0)
        idx = 0;
    s2 += utf8_index(s2, idx);

    l1bytelen = utf8_index(s1, l1);

    for (i = idx; i <= l2 - l1; i++) {
        int c;
        if (memcmp(s2, s1, l1bytelen) == 0) {
            return i;
        }
        s2 += utf8_tounicode(s2, &c);
    }
    return -1;
}

static int JimStringLast(const char *s1, int l1, const char *s2, int l2)
{
    const char *p;

    if (!l1 || !l2 || l1 > l2)
        return -1;


    for (p = s2 + l2 - 1; p != s2 - 1; p--) {
        if (*p == *s1 && memcmp(s1, p, l1) == 0) {
            return p - s2;
        }
    }
    return -1;
}

#ifdef JIM_UTF8
static int JimStringLastUtf8(const char *s1, int l1, const char *s2, int l2)
{
    int n = JimStringLast(s1, utf8_index(s1, l1), s2, utf8_index(s2, l2));
    if (n > 0) {
        n = utf8_strlen(s2, n);
    }
    return n;
}
#endif

static int JimCheckConversion(const char *str, const char *endptr)
{
    if (str[0] == '\0' || str == endptr) {
        return JIM_ERR;
    }

    if (endptr[0] != '\0') {
        while (*endptr) {
            if (!isspace(UCHAR(*endptr))) {
                return JIM_ERR;
            }
            endptr++;
        }
    }
    return JIM_OK;
}

static int JimNumberBase(const char *str, int *base, int *sign)
{
    int i = 0;

    *base = 10;

    while (isspace(UCHAR(str[i]))) {
        i++;
    }

    if (str[i] == '-') {
        *sign = -1;
        i++;
    }
    else {
        if (str[i] == '+') {
            i++;
        }
        *sign = 1;
    }

    if (str[i] != '0') {

        return 0;
    }


    switch (str[i + 1]) {
        case 'x': case 'X': *base = 16; break;
        case 'o': case 'O': *base = 8; break;
        case 'b': case 'B': *base = 2; break;
        default: return 0;
    }
    i += 2;

    if (str[i] != '-' && str[i] != '+' && !isspace(UCHAR(str[i]))) {

        return i;
    }

    *base = 10;
    return 0;
}

static long jim_strtol(const char *str, char **endptr)
{
    int sign;
    int base;
    int i = JimNumberBase(str, &base, &sign);

    if (base != 10) {
        long value = strtol(str + i, endptr, base);
        if (endptr == NULL || *endptr != str + i) {
            return value * sign;
        }
    }


    return strtol(str, endptr, 10);
}


static jim_wide jim_strtoull(const char *str, char **endptr)
{
#ifdef HAVE_LONG_LONG
    int sign;
    int base;
    int i = JimNumberBase(str, &base, &sign);

    if (base != 10) {
        jim_wide value = strtoull(str + i, endptr, base);
        if (endptr == NULL || *endptr != str + i) {
            return value * sign;
        }
    }


    return strtoull(str, endptr, 10);
#else
    return (unsigned long)jim_strtol(str, endptr);
#endif
}

int Jim_StringToWide(const char *str, jim_wide * widePtr, int base)
{
    char *endptr;

    if (base) {
        *widePtr = strtoull(str, &endptr, base);
    }
    else {
        *widePtr = jim_strtoull(str, &endptr);
    }

    return JimCheckConversion(str, endptr);
}

int Jim_StringToDouble(const char *str, double *doublePtr)
{
    char *endptr;


    errno = 0;

    *doublePtr = strtod(str, &endptr);

    return JimCheckConversion(str, endptr);
}

static jim_wide JimPowWide(jim_wide b, jim_wide e)
{
    jim_wide res = 1;


    if (b == 1) {

        return 1;
    }
    if (e < 0) {
        if (b != -1) {
            return 0;
        }
        e = -e;
    }
    while (e)
    {
        if (e & 1) {
            res *= b;
        }
        e >>= 1;
        b *= b;
    }
    return res;
}

#ifdef JIM_DEBUG_PANIC
static void JimPanicDump(int condition, const char *fmt, ...)
{
    va_list ap;

    if (!condition) {
        return;
    }

    va_start(ap, fmt);

    fprintf(stderr, "\nJIM INTERPRETER PANIC: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n\n");
    va_end(ap);

#ifdef HAVE_BACKTRACE
    {
        void *array[40];
        int size, i;
        char **strings;

        size = backtrace(array, 40);
        strings = backtrace_symbols(array, size);
        for (i = 0; i < size; i++)
            fprintf(stderr, "[backtrace] %s\n", strings[i]);
        fprintf(stderr, "[backtrace] Include the above lines and the output\n");
        fprintf(stderr, "[backtrace] of 'nm <executable>' in the bug report.\n");
    }
#endif

    exit(1);
}
#endif


void *Jim_Alloc(int size)
{
    return size ? malloc(size) : NULL;
}

void Jim_Free(void *ptr)
{
    free(ptr);
}

void *Jim_Realloc(void *ptr, int size)
{
    return realloc(ptr, size);
}

char *Jim_StrDup(const char *s)
{
    return strdup(s);
}

char *Jim_StrDupLen(const char *s, int l)
{
    char *copy = Jim_Alloc(l + 1);

    memcpy(copy, s, l + 1);
    copy[l] = 0;
    return copy;
}



static jim_wide JimClock(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (jim_wide) tv.tv_sec * 1000000 + tv.tv_usec;
}



static void JimExpandHashTableIfNeeded(Jim_HashTable *ht);
static unsigned int JimHashTableNextPower(unsigned int size);
static Jim_HashEntry *JimInsertHashEntry(Jim_HashTable *ht, const void *key, int replace);




unsigned int Jim_IntHashFunction(unsigned int key)
{
    key += ~(key << 15);
    key ^= (key >> 10);
    key += (key << 3);
    key ^= (key >> 6);
    key += ~(key << 11);
    key ^= (key >> 16);
    return key;
}

unsigned int Jim_GenHashFunction(const unsigned char *buf, int len)
{
    unsigned int h = 0;

    while (len--)
        h += (h << 3) + *buf++;
    return h;
}




static void JimResetHashTable(Jim_HashTable *ht)
{
    ht->table = NULL;
    ht->size = 0;
    ht->sizemask = 0;
    ht->used = 0;
    ht->collisions = 0;
#ifdef JIM_RANDOMISE_HASH
    ht->uniq = (rand() ^ time(NULL) ^ clock());
#else
    ht->uniq = 0;
#endif
}

static void JimInitHashTableIterator(Jim_HashTable *ht, Jim_HashTableIterator *iter)
{
    iter->ht = ht;
    iter->index = -1;
    iter->entry = NULL;
    iter->nextEntry = NULL;
}


int Jim_InitHashTable(Jim_HashTable *ht, const Jim_HashTableType *type, void *privDataPtr)
{
    JimResetHashTable(ht);
    ht->type = type;
    ht->privdata = privDataPtr;
    return JIM_OK;
}

void Jim_ResizeHashTable(Jim_HashTable *ht)
{
    int minimal = ht->used;

    if (minimal < JIM_HT_INITIAL_SIZE)
        minimal = JIM_HT_INITIAL_SIZE;
    Jim_ExpandHashTable(ht, minimal);
}


void Jim_ExpandHashTable(Jim_HashTable *ht, unsigned int size)
{
    Jim_HashTable n;
    unsigned int realsize = JimHashTableNextPower(size), i;

     if (size <= ht->used)
        return;

    Jim_InitHashTable(&n, ht->type, ht->privdata);
    n.size = realsize;
    n.sizemask = realsize - 1;
    n.table = Jim_Alloc(realsize * sizeof(Jim_HashEntry *));

    n.uniq = ht->uniq;


    memset(n.table, 0, realsize * sizeof(Jim_HashEntry *));

    n.used = ht->used;
    for (i = 0; ht->used > 0; i++) {
        Jim_HashEntry *he, *nextHe;

        if (ht->table[i] == NULL)
            continue;


        he = ht->table[i];
        while (he) {
            unsigned int h;

            nextHe = he->next;

            h = Jim_HashKey(ht, he->key) & n.sizemask;
            he->next = n.table[h];
            n.table[h] = he;
            ht->used--;

            he = nextHe;
        }
    }
    assert(ht->used == 0);
    Jim_Free(ht->table);


    *ht = n;
}


int Jim_AddHashEntry(Jim_HashTable *ht, const void *key, void *val)
{
    Jim_HashEntry *entry;

    entry = JimInsertHashEntry(ht, key, 0);
    if (entry == NULL)
        return JIM_ERR;


    Jim_SetHashKey(ht, entry, key);
    Jim_SetHashVal(ht, entry, val);
    return JIM_OK;
}


int Jim_ReplaceHashEntry(Jim_HashTable *ht, const void *key, void *val)
{
    int existed;
    Jim_HashEntry *entry;

    entry = JimInsertHashEntry(ht, key, 1);
    if (entry->key) {
        if (ht->type->valDestructor && ht->type->valDup) {
            void *newval = ht->type->valDup(ht->privdata, val);
            ht->type->valDestructor(ht->privdata, entry->u.val);
            entry->u.val = newval;
        }
        else {
            Jim_FreeEntryVal(ht, entry);
            Jim_SetHashVal(ht, entry, val);
        }
        existed = 1;
    }
    else {

        Jim_SetHashKey(ht, entry, key);
        Jim_SetHashVal(ht, entry, val);
        existed = 0;
    }

    return existed;
}


int Jim_DeleteHashEntry(Jim_HashTable *ht, const void *key)
{
    unsigned int h;
    Jim_HashEntry *he, *prevHe;

    if (ht->used == 0)
        return JIM_ERR;
    h = Jim_HashKey(ht, key) & ht->sizemask;
    he = ht->table[h];

    prevHe = NULL;
    while (he) {
        if (Jim_CompareHashKeys(ht, key, he->key)) {

            if (prevHe)
                prevHe->next = he->next;
            else
                ht->table[h] = he->next;
            Jim_FreeEntryKey(ht, he);
            Jim_FreeEntryVal(ht, he);
            Jim_Free(he);
            ht->used--;
            return JIM_OK;
        }
        prevHe = he;
        he = he->next;
    }
    return JIM_ERR;
}


int Jim_FreeHashTable(Jim_HashTable *ht)
{
    unsigned int i;


    for (i = 0; ht->used > 0; i++) {
        Jim_HashEntry *he, *nextHe;

        if ((he = ht->table[i]) == NULL)
            continue;
        while (he) {
            nextHe = he->next;
            Jim_FreeEntryKey(ht, he);
            Jim_FreeEntryVal(ht, he);
            Jim_Free(he);
            ht->used--;
            he = nextHe;
        }
    }

    Jim_Free(ht->table);

    JimResetHashTable(ht);
    return JIM_OK;
}

Jim_HashEntry *Jim_FindHashEntry(Jim_HashTable *ht, const void *key)
{
    Jim_HashEntry *he;
    unsigned int h;

    if (ht->used == 0)
        return NULL;
    h = Jim_HashKey(ht, key) & ht->sizemask;
    he = ht->table[h];
    while (he) {
        if (Jim_CompareHashKeys(ht, key, he->key))
            return he;
        he = he->next;
    }
    return NULL;
}

Jim_HashTableIterator *Jim_GetHashTableIterator(Jim_HashTable *ht)
{
    Jim_HashTableIterator *iter = Jim_Alloc(sizeof(*iter));
    JimInitHashTableIterator(ht, iter);
    return iter;
}

Jim_HashEntry *Jim_NextHashEntry(Jim_HashTableIterator *iter)
{
    while (1) {
        if (iter->entry == NULL) {
            iter->index++;
            if (iter->index >= (signed)iter->ht->size)
                break;
            iter->entry = iter->ht->table[iter->index];
        }
        else {
            iter->entry = iter->nextEntry;
        }
        if (iter->entry) {
            iter->nextEntry = iter->entry->next;
            return iter->entry;
        }
    }
    return NULL;
}




static void JimExpandHashTableIfNeeded(Jim_HashTable *ht)
{
    if (ht->size == 0)
        Jim_ExpandHashTable(ht, JIM_HT_INITIAL_SIZE);
    if (ht->size == ht->used)
        Jim_ExpandHashTable(ht, ht->size * 2);
}


static unsigned int JimHashTableNextPower(unsigned int size)
{
    unsigned int i = JIM_HT_INITIAL_SIZE;

    if (size >= 2147483648U)
        return 2147483648U;
    while (1) {
        if (i >= size)
            return i;
        i *= 2;
    }
}

static Jim_HashEntry *JimInsertHashEntry(Jim_HashTable *ht, const void *key, int replace)
{
    unsigned int h;
    Jim_HashEntry *he;


    JimExpandHashTableIfNeeded(ht);


    h = Jim_HashKey(ht, key) & ht->sizemask;

    he = ht->table[h];
    while (he) {
        if (Jim_CompareHashKeys(ht, key, he->key))
            return replace ? he : NULL;
        he = he->next;
    }


    he = Jim_Alloc(sizeof(*he));
    he->next = ht->table[h];
    ht->table[h] = he;
    ht->used++;
    he->key = NULL;

    return he;
}



static unsigned int JimStringCopyHTHashFunction(const void *key)
{
    return Jim_GenHashFunction(key, strlen(key));
}

static void *JimStringCopyHTDup(void *privdata, const void *key)
{
    return Jim_StrDup(key);
}

static int JimStringCopyHTKeyCompare(void *privdata, const void *key1, const void *key2)
{
    return strcmp(key1, key2) == 0;
}

static void JimStringCopyHTKeyDestructor(void *privdata, void *key)
{
    Jim_Free(key);
}

static const Jim_HashTableType JimPackageHashTableType = {
    JimStringCopyHTHashFunction,
    JimStringCopyHTDup,
    NULL,
    JimStringCopyHTKeyCompare,
    JimStringCopyHTKeyDestructor,
    NULL
};

typedef struct AssocDataValue
{
    Jim_InterpDeleteProc *delProc;
    void *data;
} AssocDataValue;

static void JimAssocDataHashTableValueDestructor(void *privdata, void *data)
{
    AssocDataValue *assocPtr = (AssocDataValue *) data;

    if (assocPtr->delProc != NULL)
        assocPtr->delProc((Jim_Interp *)privdata, assocPtr->data);
    Jim_Free(data);
}

static const Jim_HashTableType JimAssocDataHashTableType = {
    JimStringCopyHTHashFunction,
    JimStringCopyHTDup,
    NULL,
    JimStringCopyHTKeyCompare,
    JimStringCopyHTKeyDestructor,
    JimAssocDataHashTableValueDestructor
};

void Jim_InitStack(Jim_Stack *stack)
{
    stack->len = 0;
    stack->maxlen = 0;
    stack->vector = NULL;
}

void Jim_FreeStack(Jim_Stack *stack)
{
    Jim_Free(stack->vector);
}

int Jim_StackLen(Jim_Stack *stack)
{
    return stack->len;
}

void Jim_StackPush(Jim_Stack *stack, void *element)
{
    int neededLen = stack->len + 1;

    if (neededLen > stack->maxlen) {
        stack->maxlen = neededLen < 20 ? 20 : neededLen * 2;
        stack->vector = Jim_Realloc(stack->vector, sizeof(void *) * stack->maxlen);
    }
    stack->vector[stack->len] = element;
    stack->len++;
}

void *Jim_StackPop(Jim_Stack *stack)
{
    if (stack->len == 0)
        return NULL;
    stack->len--;
    return stack->vector[stack->len];
}

void *Jim_StackPeek(Jim_Stack *stack)
{
    if (stack->len == 0)
        return NULL;
    return stack->vector[stack->len - 1];
}

void Jim_FreeStackElements(Jim_Stack *stack, void (*freeFunc) (void *ptr))
{
    int i;

    for (i = 0; i < stack->len; i++)
        freeFunc(stack->vector[i]);
}



#define JIM_TT_NONE    0
#define JIM_TT_STR     1
#define JIM_TT_ESC     2
#define JIM_TT_VAR     3
#define JIM_TT_DICTSUGAR   4
#define JIM_TT_CMD     5

#define JIM_TT_SEP     6
#define JIM_TT_EOL     7
#define JIM_TT_EOF     8

#define JIM_TT_LINE    9
#define JIM_TT_WORD   10


#define JIM_TT_SUBEXPR_START  11
#define JIM_TT_SUBEXPR_END    12
#define JIM_TT_SUBEXPR_COMMA  13
#define JIM_TT_EXPR_INT       14
#define JIM_TT_EXPR_DOUBLE    15
#define JIM_TT_EXPR_BOOLEAN   16

#define JIM_TT_EXPRSUGAR      17


#define JIM_TT_EXPR_OP        20

#define TOKEN_IS_SEP(type) (type >= JIM_TT_SEP && type <= JIM_TT_EOF)

#define TOKEN_IS_EXPR_START(type) (type == JIM_TT_NONE || type == JIM_TT_SUBEXPR_START || type == JIM_TT_SUBEXPR_COMMA)

#define TOKEN_IS_EXPR_OP(type) (type >= JIM_TT_EXPR_OP)

struct JimParseMissing {
    int ch;
    int line;
};

struct JimParserCtx
{
    const char *p;
    int len;
    int linenr;
    const char *tstart;
    const char *tend;
    int tline;
    int tt;
    int eof;
    int inquote;
    int comment;
    struct JimParseMissing missing;
};

static int JimParseScript(struct JimParserCtx *pc);
static int JimParseSep(struct JimParserCtx *pc);
static int JimParseEol(struct JimParserCtx *pc);
static int JimParseCmd(struct JimParserCtx *pc);
static int JimParseQuote(struct JimParserCtx *pc);
static int JimParseVar(struct JimParserCtx *pc);
static int JimParseBrace(struct JimParserCtx *pc);
static int JimParseStr(struct JimParserCtx *pc);
static int JimParseComment(struct JimParserCtx *pc);
static void JimParseSubCmd(struct JimParserCtx *pc);
static int JimParseSubQuote(struct JimParserCtx *pc);
static Jim_Obj *JimParserGetTokenObj(Jim_Interp *interp, struct JimParserCtx *pc);

static void JimParserInit(struct JimParserCtx *pc, const char *prg, int len, int linenr)
{
    pc->p = prg;
    pc->len = len;
    pc->tstart = NULL;
    pc->tend = NULL;
    pc->tline = 0;
    pc->tt = JIM_TT_NONE;
    pc->eof = 0;
    pc->inquote = 0;
    pc->linenr = linenr;
    pc->comment = 1;
    pc->missing.ch = ' ';
    pc->missing.line = linenr;
}

static int JimParseScript(struct JimParserCtx *pc)
{
    while (1) {
        if (!pc->len) {
            pc->tstart = pc->p;
            pc->tend = pc->p - 1;
            pc->tline = pc->linenr;
            pc->tt = JIM_TT_EOL;
            pc->eof = 1;
            return JIM_OK;
        }
        switch (*(pc->p)) {
            case '\\':
                if (*(pc->p + 1) == '\n' && !pc->inquote) {
                    return JimParseSep(pc);
                }
                pc->comment = 0;
                return JimParseStr(pc);
            case ' ':
            case '\t':
            case '\r':
            case '\f':
                if (!pc->inquote)
                    return JimParseSep(pc);
                pc->comment = 0;
                return JimParseStr(pc);
            case '\n':
            case ';':
                pc->comment = 1;
                if (!pc->inquote)
                    return JimParseEol(pc);
                return JimParseStr(pc);
            case '[':
                pc->comment = 0;
                return JimParseCmd(pc);
            case '$':
                pc->comment = 0;
                if (JimParseVar(pc) == JIM_ERR) {

                    pc->tstart = pc->tend = pc->p++;
                    pc->len--;
                    pc->tt = JIM_TT_ESC;
                }
                return JIM_OK;
            case '#':
                if (pc->comment) {
                    JimParseComment(pc);
                    continue;
                }
                return JimParseStr(pc);
            default:
                pc->comment = 0;
                return JimParseStr(pc);
        }
        return JIM_OK;
    }
}

static int JimParseSep(struct JimParserCtx *pc)
{
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    while (isspace(UCHAR(*pc->p)) || (*pc->p == '\\' && *(pc->p + 1) == '\n')) {
        if (*pc->p == '\n') {
            break;
        }
        if (*pc->p == '\\') {
            pc->p++;
            pc->len--;
            pc->linenr++;
        }
        pc->p++;
        pc->len--;
    }
    pc->tend = pc->p - 1;
    pc->tt = JIM_TT_SEP;
    return JIM_OK;
}

static int JimParseEol(struct JimParserCtx *pc)
{
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    while (isspace(UCHAR(*pc->p)) || *pc->p == ';') {
        if (*pc->p == '\n')
            pc->linenr++;
        pc->p++;
        pc->len--;
    }
    pc->tend = pc->p - 1;
    pc->tt = JIM_TT_EOL;
    return JIM_OK;
}


static void JimParseSubBrace(struct JimParserCtx *pc)
{
    int level = 1;


    pc->p++;
    pc->len--;
    while (pc->len) {
        switch (*pc->p) {
            case '\\':
                if (pc->len > 1) {
                    if (*++pc->p == '\n') {
                        pc->linenr++;
                    }
                    pc->len--;
                }
                break;

            case '{':
                level++;
                break;

            case '}':
                if (--level == 0) {
                    pc->tend = pc->p - 1;
                    pc->p++;
                    pc->len--;
                    return;
                }
                break;

            case '\n':
                pc->linenr++;
                break;
        }
        pc->p++;
        pc->len--;
    }
    pc->missing.ch = '{';
    pc->missing.line = pc->tline;
    pc->tend = pc->p - 1;
}

static int JimParseSubQuote(struct JimParserCtx *pc)
{
    int tt = JIM_TT_STR;
    int line = pc->tline;


    pc->p++;
    pc->len--;
    while (pc->len) {
        switch (*pc->p) {
            case '\\':
                if (pc->len > 1) {
                    if (*++pc->p == '\n') {
                        pc->linenr++;
                    }
                    pc->len--;
                    tt = JIM_TT_ESC;
                }
                break;

            case '"':
                pc->tend = pc->p - 1;
                pc->p++;
                pc->len--;
                return tt;

            case '[':
                JimParseSubCmd(pc);
                tt = JIM_TT_ESC;
                continue;

            case '\n':
                pc->linenr++;
                break;

            case '$':
                tt = JIM_TT_ESC;
                break;
        }
        pc->p++;
        pc->len--;
    }
    pc->missing.ch = '"';
    pc->missing.line = line;
    pc->tend = pc->p - 1;
    return tt;
}

static void JimParseSubCmd(struct JimParserCtx *pc)
{
    int level = 1;
    int startofword = 1;
    int line = pc->tline;


    pc->p++;
    pc->len--;
    while (pc->len) {
        switch (*pc->p) {
            case '\\':
                if (pc->len > 1) {
                    if (*++pc->p == '\n') {
                        pc->linenr++;
                    }
                    pc->len--;
                }
                break;

            case '[':
                level++;
                break;

            case ']':
                if (--level == 0) {
                    pc->tend = pc->p - 1;
                    pc->p++;
                    pc->len--;
                    return;
                }
                break;

            case '"':
                if (startofword) {
                    JimParseSubQuote(pc);
                    continue;
                }
                break;

            case '{':
                JimParseSubBrace(pc);
                startofword = 0;
                continue;

            case '\n':
                pc->linenr++;
                break;
        }
        startofword = isspace(UCHAR(*pc->p));
        pc->p++;
        pc->len--;
    }
    pc->missing.ch = '[';
    pc->missing.line = line;
    pc->tend = pc->p - 1;
}

static int JimParseBrace(struct JimParserCtx *pc)
{
    pc->tstart = pc->p + 1;
    pc->tline = pc->linenr;
    pc->tt = JIM_TT_STR;
    JimParseSubBrace(pc);
    return JIM_OK;
}

static int JimParseCmd(struct JimParserCtx *pc)
{
    pc->tstart = pc->p + 1;
    pc->tline = pc->linenr;
    pc->tt = JIM_TT_CMD;
    JimParseSubCmd(pc);
    return JIM_OK;
}

static int JimParseQuote(struct JimParserCtx *pc)
{
    pc->tstart = pc->p + 1;
    pc->tline = pc->linenr;
    pc->tt = JimParseSubQuote(pc);
    return JIM_OK;
}

static int JimParseVar(struct JimParserCtx *pc)
{

    pc->p++;
    pc->len--;

#ifdef EXPRSUGAR_BRACKET
    if (*pc->p == '[') {

        JimParseCmd(pc);
        pc->tt = JIM_TT_EXPRSUGAR;
        return JIM_OK;
    }
#endif

    pc->tstart = pc->p;
    pc->tt = JIM_TT_VAR;
    pc->tline = pc->linenr;

    if (*pc->p == '{') {
        pc->tstart = ++pc->p;
        pc->len--;

        while (pc->len && *pc->p != '}') {
            if (*pc->p == '\n') {
                pc->linenr++;
            }
            pc->p++;
            pc->len--;
        }
        pc->tend = pc->p - 1;
        if (pc->len) {
            pc->p++;
            pc->len--;
        }
    }
    else {
        while (1) {

            if (pc->p[0] == ':' && pc->p[1] == ':') {
                while (*pc->p == ':') {
                    pc->p++;
                    pc->len--;
                }
                continue;
            }
            if (isalnum(UCHAR(*pc->p)) || *pc->p == '_' || UCHAR(*pc->p) >= 0x80) {
                pc->p++;
                pc->len--;
                continue;
            }
            break;
        }

        if (*pc->p == '(') {
            int count = 1;
            const char *paren = NULL;

            pc->tt = JIM_TT_DICTSUGAR;

            while (count && pc->len) {
                pc->p++;
                pc->len--;
                if (*pc->p == '\\' && pc->len >= 1) {
                    pc->p++;
                    pc->len--;
                }
                else if (*pc->p == '(') {
                    count++;
                }
                else if (*pc->p == ')') {
                    paren = pc->p;
                    count--;
                }
            }
            if (count == 0) {
                pc->p++;
                pc->len--;
            }
            else if (paren) {

                paren++;
                pc->len += (pc->p - paren);
                pc->p = paren;
            }
#ifndef EXPRSUGAR_BRACKET
            if (*pc->tstart == '(') {
                pc->tt = JIM_TT_EXPRSUGAR;
            }
#endif
        }
        pc->tend = pc->p - 1;
    }
    if (pc->tstart == pc->p) {
        pc->p--;
        pc->len++;
        return JIM_ERR;
    }
    return JIM_OK;
}

static int JimParseStr(struct JimParserCtx *pc)
{
    if (pc->tt == JIM_TT_SEP || pc->tt == JIM_TT_EOL ||
        pc->tt == JIM_TT_NONE || pc->tt == JIM_TT_STR) {

        if (*pc->p == '{') {
            return JimParseBrace(pc);
        }
        if (*pc->p == '"') {
            pc->inquote = 1;
            pc->p++;
            pc->len--;

            pc->missing.line = pc->tline;
        }
    }
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    while (1) {
        if (pc->len == 0) {
            if (pc->inquote) {
                pc->missing.ch = '"';
            }
            pc->tend = pc->p - 1;
            pc->tt = JIM_TT_ESC;
            return JIM_OK;
        }
        switch (*pc->p) {
            case '\\':
                if (!pc->inquote && *(pc->p + 1) == '\n') {
                    pc->tend = pc->p - 1;
                    pc->tt = JIM_TT_ESC;
                    return JIM_OK;
                }
                if (pc->len >= 2) {
                    if (*(pc->p + 1) == '\n') {
                        pc->linenr++;
                    }
                    pc->p++;
                    pc->len--;
                }
                else if (pc->len == 1) {

                    pc->missing.ch = '\\';
                }
                break;
            case '(':

                if (pc->len > 1 && pc->p[1] != '$') {
                    break;
                }

            case ')':

                if (*pc->p == '(' || pc->tt == JIM_TT_VAR) {
                    if (pc->p == pc->tstart) {

                        pc->p++;
                        pc->len--;
                    }
                    pc->tend = pc->p - 1;
                    pc->tt = JIM_TT_ESC;
                    return JIM_OK;
                }
                break;

            case '$':
            case '[':
                pc->tend = pc->p - 1;
                pc->tt = JIM_TT_ESC;
                return JIM_OK;
            case ' ':
            case '\t':
            case '\n':
            case '\r':
            case '\f':
            case ';':
                if (!pc->inquote) {
                    pc->tend = pc->p - 1;
                    pc->tt = JIM_TT_ESC;
                    return JIM_OK;
                }
                else if (*pc->p == '\n') {
                    pc->linenr++;
                }
                break;
            case '"':
                if (pc->inquote) {
                    pc->tend = pc->p - 1;
                    pc->tt = JIM_TT_ESC;
                    pc->p++;
                    pc->len--;
                    pc->inquote = 0;
                    return JIM_OK;
                }
                break;
        }
        pc->p++;
        pc->len--;
    }
    return JIM_OK;
}

static int JimParseComment(struct JimParserCtx *pc)
{
    while (*pc->p) {
        if (*pc->p == '\\') {
            pc->p++;
            pc->len--;
            if (pc->len == 0) {
                pc->missing.ch = '\\';
                return JIM_OK;
            }
            if (*pc->p == '\n') {
                pc->linenr++;
            }
        }
        else if (*pc->p == '\n') {
            pc->p++;
            pc->len--;
            pc->linenr++;
            break;
        }
        pc->p++;
        pc->len--;
    }
    return JIM_OK;
}


static int xdigitval(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static int odigitval(int c)
{
    if (c >= '0' && c <= '7')
        return c - '0';
    return -1;
}

static int JimEscape(char *dest, const char *s, int slen)
{
    char *p = dest;
    int i, len;

    for (i = 0; i < slen; i++) {
        switch (s[i]) {
            case '\\':
                switch (s[i + 1]) {
                    case 'a':
                        *p++ = 0x7;
                        i++;
                        break;
                    case 'b':
                        *p++ = 0x8;
                        i++;
                        break;
                    case 'f':
                        *p++ = 0xc;
                        i++;
                        break;
                    case 'n':
                        *p++ = 0xa;
                        i++;
                        break;
                    case 'r':
                        *p++ = 0xd;
                        i++;
                        break;
                    case 't':
                        *p++ = 0x9;
                        i++;
                        break;
                    case 'u':
                    case 'U':
                    case 'x':
                        {
                            unsigned val = 0;
                            int k;
                            int maxchars = 2;

                            i++;

                            if (s[i] == 'U') {
                                maxchars = 8;
                            }
                            else if (s[i] == 'u') {
                                if (s[i + 1] == '{') {
                                    maxchars = 6;
                                    i++;
                                }
                                else {
                                    maxchars = 4;
                                }
                            }

                            for (k = 0; k < maxchars; k++) {
                                int c = xdigitval(s[i + k + 1]);
                                if (c == -1) {
                                    break;
                                }
                                val = (val << 4) | c;
                            }

                            if (s[i] == '{') {
                                if (k == 0 || val > 0x1fffff || s[i + k + 1] != '}') {

                                    i--;
                                    k = 0;
                                }
                                else {

                                    k++;
                                }
                            }
                            if (k) {

                                if (s[i] == 'x') {
                                    *p++ = val;
                                }
                                else {
                                    p += utf8_fromunicode(p, val);
                                }
                                i += k;
                                break;
                            }

                            *p++ = s[i];
                        }
                        break;
                    case 'v':
                        *p++ = 0xb;
                        i++;
                        break;
                    case '\0':
                        *p++ = '\\';
                        i++;
                        break;
                    case '\n':

                        *p++ = ' ';
                        do {
                            i++;
                        } while (s[i + 1] == ' ' || s[i + 1] == '\t');
                        break;
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':

                        {
                            int val = 0;
                            int c = odigitval(s[i + 1]);

                            val = c;
                            c = odigitval(s[i + 2]);
                            if (c == -1) {
                                *p++ = val;
                                i++;
                                break;
                            }
                            val = (val * 8) + c;
                            c = odigitval(s[i + 3]);
                            if (c == -1) {
                                *p++ = val;
                                i += 2;
                                break;
                            }
                            val = (val * 8) + c;
                            *p++ = val;
                            i += 3;
                        }
                        break;
                    default:
                        *p++ = s[i + 1];
                        i++;
                        break;
                }
                break;
            default:
                *p++ = s[i];
                break;
        }
    }
    len = p - dest;
    *p = '\0';
    return len;
}

static Jim_Obj *JimParserGetTokenObj(Jim_Interp *interp, struct JimParserCtx *pc)
{
    const char *start, *end;
    char *token;
    int len;

    start = pc->tstart;
    end = pc->tend;
    len = (end - start) + 1;
    if (len < 0) {
        len = 0;
    }
    token = Jim_Alloc(len + 1);
    if (pc->tt != JIM_TT_ESC) {

        memcpy(token, start, len);
        token[len] = '\0';
    }
    else {

        len = JimEscape(token, start, len);
    }

    return Jim_NewStringObjNoAlloc(interp, token, len);
}

static int JimParseListSep(struct JimParserCtx *pc);
static int JimParseListStr(struct JimParserCtx *pc);
static int JimParseListQuote(struct JimParserCtx *pc);

static int JimParseList(struct JimParserCtx *pc)
{
    if (isspace(UCHAR(*pc->p))) {
        return JimParseListSep(pc);
    }
    switch (*pc->p) {
        case '"':
            return JimParseListQuote(pc);

        case '{':
            return JimParseBrace(pc);

        default:
            if (pc->len) {
                return JimParseListStr(pc);
            }
            break;
    }

    pc->tstart = pc->tend = pc->p;
    pc->tline = pc->linenr;
    pc->tt = JIM_TT_EOL;
    pc->eof = 1;
    return JIM_OK;
}

static int JimParseListSep(struct JimParserCtx *pc)
{
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    while (isspace(UCHAR(*pc->p))) {
        if (*pc->p == '\n') {
            pc->linenr++;
        }
        pc->p++;
        pc->len--;
    }
    pc->tend = pc->p - 1;
    pc->tt = JIM_TT_SEP;
    return JIM_OK;
}

static int JimParseListQuote(struct JimParserCtx *pc)
{
    pc->p++;
    pc->len--;

    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    pc->tt = JIM_TT_STR;

    while (pc->len) {
        switch (*pc->p) {
            case '\\':
                pc->tt = JIM_TT_ESC;
                if (--pc->len == 0) {

                    pc->tend = pc->p;
                    return JIM_OK;
                }
                pc->p++;
                break;
            case '\n':
                pc->linenr++;
                break;
            case '"':
                pc->tend = pc->p - 1;
                pc->p++;
                pc->len--;
                return JIM_OK;
        }
        pc->p++;
        pc->len--;
    }

    pc->tend = pc->p - 1;
    return JIM_OK;
}

static int JimParseListStr(struct JimParserCtx *pc)
{
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    pc->tt = JIM_TT_STR;

    while (pc->len) {
        if (isspace(UCHAR(*pc->p))) {
            pc->tend = pc->p - 1;
            return JIM_OK;
        }
        if (*pc->p == '\\') {
            if (--pc->len == 0) {

                pc->tend = pc->p;
                return JIM_OK;
            }
            pc->tt = JIM_TT_ESC;
            pc->p++;
        }
        pc->p++;
        pc->len--;
    }
    pc->tend = pc->p - 1;
    return JIM_OK;
}



Jim_Obj *Jim_NewObj(Jim_Interp *interp)
{
    Jim_Obj *objPtr;


    if (interp->freeList != NULL) {

        objPtr = interp->freeList;
        interp->freeList = objPtr->nextObjPtr;
    }
    else {

        objPtr = Jim_Alloc(sizeof(*objPtr));
    }

    objPtr->refCount = 0;


    objPtr->prevObjPtr = NULL;
    objPtr->nextObjPtr = interp->liveList;
    if (interp->liveList)
        interp->liveList->prevObjPtr = objPtr;
    interp->liveList = objPtr;

    return objPtr;
}

void Jim_FreeObj(Jim_Interp *interp, Jim_Obj *objPtr)
{

    JimPanic((objPtr->refCount != 0, "!!!Object %p freed with bad refcount %d, type=%s", objPtr,
        objPtr->refCount, objPtr->typePtr ? objPtr->typePtr->name : "<none>"));


    Jim_FreeIntRep(interp, objPtr);

    if (objPtr->bytes != NULL) {
        if (objPtr->bytes != JimEmptyStringRep)
            Jim_Free(objPtr->bytes);
    }

    if (objPtr->prevObjPtr)
        objPtr->prevObjPtr->nextObjPtr = objPtr->nextObjPtr;
    if (objPtr->nextObjPtr)
        objPtr->nextObjPtr->prevObjPtr = objPtr->prevObjPtr;
    if (interp->liveList == objPtr)
        interp->liveList = objPtr->nextObjPtr;
#ifdef JIM_DISABLE_OBJECT_POOL
    Jim_Free(objPtr);
#else

    objPtr->prevObjPtr = NULL;
    objPtr->nextObjPtr = interp->freeList;
    if (interp->freeList)
        interp->freeList->prevObjPtr = objPtr;
    interp->freeList = objPtr;
    objPtr->refCount = -1;
#endif
}


void Jim_InvalidateStringRep(Jim_Obj *objPtr)
{
    if (objPtr->bytes != NULL) {
        if (objPtr->bytes != JimEmptyStringRep)
            Jim_Free(objPtr->bytes);
    }
    objPtr->bytes = NULL;
}


Jim_Obj *Jim_DuplicateObj(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_Obj *dupPtr;

    dupPtr = Jim_NewObj(interp);
    if (objPtr->bytes == NULL) {

        dupPtr->bytes = NULL;
    }
    else if (objPtr->length == 0) {
        dupPtr->bytes = JimEmptyStringRep;
        dupPtr->length = 0;
        dupPtr->typePtr = NULL;
        return dupPtr;
    }
    else {
        dupPtr->bytes = Jim_Alloc(objPtr->length + 1);
        dupPtr->length = objPtr->length;

        memcpy(dupPtr->bytes, objPtr->bytes, objPtr->length + 1);
    }


    dupPtr->typePtr = objPtr->typePtr;
    if (objPtr->typePtr != NULL) {
        if (objPtr->typePtr->dupIntRepProc == NULL) {
            dupPtr->internalRep = objPtr->internalRep;
        }
        else {

            objPtr->typePtr->dupIntRepProc(interp, objPtr, dupPtr);
        }
    }
    return dupPtr;
}

const char *Jim_GetString(Jim_Obj *objPtr, int *lenPtr)
{
    if (objPtr->bytes == NULL) {

        JimPanic((objPtr->typePtr->updateStringProc == NULL, "UpdateStringProc called against '%s' type.", objPtr->typePtr->name));
        objPtr->typePtr->updateStringProc(objPtr);
    }
    if (lenPtr)
        *lenPtr = objPtr->length;
    return objPtr->bytes;
}


int Jim_Length(Jim_Obj *objPtr)
{
    if (objPtr->bytes == NULL) {

        Jim_GetString(objPtr, NULL);
    }
    return objPtr->length;
}


const char *Jim_String(Jim_Obj *objPtr)
{
    if (objPtr->bytes == NULL) {

        Jim_GetString(objPtr, NULL);
    }
    return objPtr->bytes;
}

static void JimSetStringBytes(Jim_Obj *objPtr, const char *str)
{
    objPtr->bytes = Jim_StrDup(str);
    objPtr->length = strlen(str);
}

static void FreeDictSubstInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void DupDictSubstInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);

static const Jim_ObjType dictSubstObjType = {
    "dict-substitution",
    FreeDictSubstInternalRep,
    DupDictSubstInternalRep,
    NULL,
    JIM_TYPE_NONE,
};

static void FreeInterpolatedInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void DupInterpolatedInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);

static const Jim_ObjType interpolatedObjType = {
    "interpolated",
    FreeInterpolatedInternalRep,
    DupInterpolatedInternalRep,
    NULL,
    JIM_TYPE_NONE,
};

static void FreeInterpolatedInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_DecrRefCount(interp, objPtr->internalRep.dictSubstValue.indexObjPtr);
}

static void DupInterpolatedInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{

    dupPtr->internalRep = srcPtr->internalRep;

    Jim_IncrRefCount(dupPtr->internalRep.dictSubstValue.indexObjPtr);
}

static void DupStringInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);
static int SetStringFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr);

static const Jim_ObjType stringObjType = {
    "string",
    NULL,
    DupStringInternalRep,
    NULL,
    JIM_TYPE_REFERENCES,
};

static void DupStringInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    JIM_NOTUSED(interp);

    dupPtr->internalRep.strValue.maxLength = srcPtr->length;
    dupPtr->internalRep.strValue.charLength = srcPtr->internalRep.strValue.charLength;
}

static int SetStringFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    if (objPtr->typePtr != &stringObjType) {

        if (objPtr->bytes == NULL) {

            JimPanic((objPtr->typePtr->updateStringProc == NULL, "UpdateStringProc called against '%s' type.", objPtr->typePtr->name));
            objPtr->typePtr->updateStringProc(objPtr);
        }

        Jim_FreeIntRep(interp, objPtr);

        objPtr->typePtr = &stringObjType;
        objPtr->internalRep.strValue.maxLength = objPtr->length;

        objPtr->internalRep.strValue.charLength = -1;
    }
    return JIM_OK;
}

int Jim_Utf8Length(Jim_Interp *interp, Jim_Obj *objPtr)
{
#ifdef JIM_UTF8
    SetStringFromAny(interp, objPtr);

    if (objPtr->internalRep.strValue.charLength < 0) {
        objPtr->internalRep.strValue.charLength = utf8_strlen(objPtr->bytes, objPtr->length);
    }
    return objPtr->internalRep.strValue.charLength;
#else
    return Jim_Length(objPtr);
#endif
}


Jim_Obj *Jim_NewStringObj(Jim_Interp *interp, const char *s, int len)
{
    Jim_Obj *objPtr = Jim_NewObj(interp);


    if (len == -1)
        len = strlen(s);

    if (len == 0) {
        objPtr->bytes = JimEmptyStringRep;
    }
    else {
        objPtr->bytes = Jim_StrDupLen(s, len);
    }
    objPtr->length = len;


    objPtr->typePtr = NULL;
    return objPtr;
}


Jim_Obj *Jim_NewStringObjUtf8(Jim_Interp *interp, const char *s, int charlen)
{
#ifdef JIM_UTF8

    int bytelen = utf8_index(s, charlen);

    Jim_Obj *objPtr = Jim_NewStringObj(interp, s, bytelen);


    objPtr->typePtr = &stringObjType;
    objPtr->internalRep.strValue.maxLength = bytelen;
    objPtr->internalRep.strValue.charLength = charlen;

    return objPtr;
#else
    return Jim_NewStringObj(interp, s, charlen);
#endif
}

Jim_Obj *Jim_NewStringObjNoAlloc(Jim_Interp *interp, char *s, int len)
{
    Jim_Obj *objPtr = Jim_NewObj(interp);

    objPtr->bytes = s;
    objPtr->length = (len == -1) ? strlen(s) : len;
    objPtr->typePtr = NULL;
    return objPtr;
}

static void StringAppendString(Jim_Obj *objPtr, const char *str, int len)
{
    int needlen;

    if (len == -1)
        len = strlen(str);
    needlen = objPtr->length + len;
    if (objPtr->internalRep.strValue.maxLength < needlen ||
        objPtr->internalRep.strValue.maxLength == 0) {
        needlen *= 2;

        if (needlen < 7) {
            needlen = 7;
        }
        if (objPtr->bytes == JimEmptyStringRep) {
            objPtr->bytes = Jim_Alloc(needlen + 1);
        }
        else {
            objPtr->bytes = Jim_Realloc(objPtr->bytes, needlen + 1);
        }
        objPtr->internalRep.strValue.maxLength = needlen;
    }
    memcpy(objPtr->bytes + objPtr->length, str, len);
    objPtr->bytes[objPtr->length + len] = '\0';

    if (objPtr->internalRep.strValue.charLength >= 0) {

        objPtr->internalRep.strValue.charLength += utf8_strlen(objPtr->bytes + objPtr->length, len);
    }
    objPtr->length += len;
}

void Jim_AppendString(Jim_Interp *interp, Jim_Obj *objPtr, const char *str, int len)
{
    JimPanic((Jim_IsShared(objPtr), "Jim_AppendString called with shared object"));
    SetStringFromAny(interp, objPtr);
    StringAppendString(objPtr, str, len);
}

void Jim_AppendObj(Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *appendObjPtr)
{
    int len;
    const char *str = Jim_GetString(appendObjPtr, &len);
    Jim_AppendString(interp, objPtr, str, len);
}

void Jim_AppendStrings(Jim_Interp *interp, Jim_Obj *objPtr, ...)
{
    va_list ap;

    SetStringFromAny(interp, objPtr);
    va_start(ap, objPtr);
    while (1) {
        const char *s = va_arg(ap, const char *);

        if (s == NULL)
            break;
        Jim_AppendString(interp, objPtr, s, -1);
    }
    va_end(ap);
}

int Jim_StringEqObj(Jim_Obj *aObjPtr, Jim_Obj *bObjPtr)
{
    if (aObjPtr == bObjPtr) {
        return 1;
    }
    else {
        int Alen, Blen;
        const char *sA = Jim_GetString(aObjPtr, &Alen);
        const char *sB = Jim_GetString(bObjPtr, &Blen);

        return Alen == Blen && memcmp(sA, sB, Alen) == 0;
    }
}

int Jim_StringMatchObj(Jim_Interp *interp, Jim_Obj *patternObjPtr, Jim_Obj *objPtr, int nocase)
{
    return JimGlobMatch(Jim_String(patternObjPtr), Jim_String(objPtr), nocase);
}

int Jim_StringCompareObj(Jim_Interp *interp, Jim_Obj *firstObjPtr, Jim_Obj *secondObjPtr, int nocase)
{
    int l1, l2;
    const char *s1 = Jim_GetString(firstObjPtr, &l1);
    const char *s2 = Jim_GetString(secondObjPtr, &l2);

    if (nocase) {

        return JimStringCompareLen(s1, s2, -1, nocase);
    }
    return JimStringCompare(s1, l1, s2, l2);
}

int Jim_StringCompareLenObj(Jim_Interp *interp, Jim_Obj *firstObjPtr, Jim_Obj *secondObjPtr, int nocase)
{
    const char *s1 = Jim_String(firstObjPtr);
    const char *s2 = Jim_String(secondObjPtr);

    return JimStringCompareLen(s1, s2, Jim_Utf8Length(interp, firstObjPtr), nocase);
}

static int JimRelToAbsIndex(int len, int idx)
{
    if (idx < 0)
        return len + idx;
    return idx;
}

static void JimRelToAbsRange(int len, int *firstPtr, int *lastPtr, int *rangeLenPtr)
{
    int rangeLen;

    if (*firstPtr > *lastPtr) {
        rangeLen = 0;
    }
    else {
        rangeLen = *lastPtr - *firstPtr + 1;
        if (rangeLen) {
            if (*firstPtr < 0) {
                rangeLen += *firstPtr;
                *firstPtr = 0;
            }
            if (*lastPtr >= len) {
                rangeLen -= (*lastPtr - (len - 1));
                *lastPtr = len - 1;
            }
        }
    }
    if (rangeLen < 0)
        rangeLen = 0;

    *rangeLenPtr = rangeLen;
}

static int JimStringGetRange(Jim_Interp *interp, Jim_Obj *firstObjPtr, Jim_Obj *lastObjPtr,
    int len, int *first, int *last, int *range)
{
    if (Jim_GetIndex(interp, firstObjPtr, first) != JIM_OK) {
        return JIM_ERR;
    }
    if (Jim_GetIndex(interp, lastObjPtr, last) != JIM_OK) {
        return JIM_ERR;
    }
    *first = JimRelToAbsIndex(len, *first);
    *last = JimRelToAbsIndex(len, *last);
    JimRelToAbsRange(len, first, last, range);
    return JIM_OK;
}

Jim_Obj *Jim_StringByteRangeObj(Jim_Interp *interp,
    Jim_Obj *strObjPtr, Jim_Obj *firstObjPtr, Jim_Obj *lastObjPtr)
{
    int first, last;
    const char *str;
    int rangeLen;
    int bytelen;

    str = Jim_GetString(strObjPtr, &bytelen);

    if (JimStringGetRange(interp, firstObjPtr, lastObjPtr, bytelen, &first, &last, &rangeLen) != JIM_OK) {
        return NULL;
    }

    if (first == 0 && rangeLen == bytelen) {
        return strObjPtr;
    }
    return Jim_NewStringObj(interp, str + first, rangeLen);
}

Jim_Obj *Jim_StringRangeObj(Jim_Interp *interp,
    Jim_Obj *strObjPtr, Jim_Obj *firstObjPtr, Jim_Obj *lastObjPtr)
{
#ifdef JIM_UTF8
    int first, last;
    const char *str;
    int len, rangeLen;
    int bytelen;

    str = Jim_GetString(strObjPtr, &bytelen);
    len = Jim_Utf8Length(interp, strObjPtr);

    if (JimStringGetRange(interp, firstObjPtr, lastObjPtr, len, &first, &last, &rangeLen) != JIM_OK) {
        return NULL;
    }

    if (first == 0 && rangeLen == len) {
        return strObjPtr;
    }
    if (len == bytelen) {

        return Jim_NewStringObj(interp, str + first, rangeLen);
    }
    return Jim_NewStringObjUtf8(interp, str + utf8_index(str, first), rangeLen);
#else
    return Jim_StringByteRangeObj(interp, strObjPtr, firstObjPtr, lastObjPtr);
#endif
}

Jim_Obj *JimStringReplaceObj(Jim_Interp *interp,
    Jim_Obj *strObjPtr, Jim_Obj *firstObjPtr, Jim_Obj *lastObjPtr, Jim_Obj *newStrObj)
{
    int first, last;
    const char *str;
    int len, rangeLen;
    Jim_Obj *objPtr;

    len = Jim_Utf8Length(interp, strObjPtr);

    if (JimStringGetRange(interp, firstObjPtr, lastObjPtr, len, &first, &last, &rangeLen) != JIM_OK) {
        return NULL;
    }

    if (last < first) {
        return strObjPtr;
    }

    str = Jim_String(strObjPtr);


    objPtr = Jim_NewStringObjUtf8(interp, str, first);


    if (newStrObj) {
        Jim_AppendObj(interp, objPtr, newStrObj);
    }


    Jim_AppendString(interp, objPtr, str + utf8_index(str, last + 1), len - last - 1);

    return objPtr;
}

static void JimStrCopyUpperLower(char *dest, const char *str, int uc)
{
    while (*str) {
        int c;
        str += utf8_tounicode(str, &c);
        dest += utf8_getchars(dest, uc ? utf8_upper(c) : utf8_lower(c));
    }
    *dest = 0;
}

static Jim_Obj *JimStringToLower(Jim_Interp *interp, Jim_Obj *strObjPtr)
{
    char *buf;
    int len;
    const char *str;

    str = Jim_GetString(strObjPtr, &len);

#ifdef JIM_UTF8
    len *= 2;
#endif
    buf = Jim_Alloc(len + 1);
    JimStrCopyUpperLower(buf, str, 0);
    return Jim_NewStringObjNoAlloc(interp, buf, -1);
}

static Jim_Obj *JimStringToUpper(Jim_Interp *interp, Jim_Obj *strObjPtr)
{
    char *buf;
    const char *str;
    int len;

    str = Jim_GetString(strObjPtr, &len);

#ifdef JIM_UTF8
    len *= 2;
#endif
    buf = Jim_Alloc(len + 1);
    JimStrCopyUpperLower(buf, str, 1);
    return Jim_NewStringObjNoAlloc(interp, buf, -1);
}

static Jim_Obj *JimStringToTitle(Jim_Interp *interp, Jim_Obj *strObjPtr)
{
    char *buf, *p;
    int len;
    int c;
    const char *str;

    str = Jim_GetString(strObjPtr, &len);

#ifdef JIM_UTF8
    len *= 2;
#endif
    buf = p = Jim_Alloc(len + 1);

    str += utf8_tounicode(str, &c);
    p += utf8_getchars(p, utf8_title(c));

    JimStrCopyUpperLower(p, str, 0);

    return Jim_NewStringObjNoAlloc(interp, buf, -1);
}

static const char *utf8_memchr(const char *str, int len, int c)
{
#ifdef JIM_UTF8
    while (len) {
        int sc;
        int n = utf8_tounicode(str, &sc);
        if (sc == c) {
            return str;
        }
        str += n;
        len -= n;
    }
    return NULL;
#else
    return memchr(str, c, len);
#endif
}

static const char *JimFindTrimLeft(const char *str, int len, const char *trimchars, int trimlen)
{
    while (len) {
        int c;
        int n = utf8_tounicode(str, &c);

        if (utf8_memchr(trimchars, trimlen, c) == NULL) {

            break;
        }
        str += n;
        len -= n;
    }
    return str;
}

static const char *JimFindTrimRight(const char *str, int len, const char *trimchars, int trimlen)
{
    str += len;

    while (len) {
        int c;
        int n = utf8_prev_len(str, len);

        len -= n;
        str -= n;

        n = utf8_tounicode(str, &c);

        if (utf8_memchr(trimchars, trimlen, c) == NULL) {
            return str + n;
        }
    }

    return NULL;
}

static const char default_trim_chars[] = " \t\n\r";

static int default_trim_chars_len = sizeof(default_trim_chars);

static Jim_Obj *JimStringTrimLeft(Jim_Interp *interp, Jim_Obj *strObjPtr, Jim_Obj *trimcharsObjPtr)
{
    int len;
    const char *str = Jim_GetString(strObjPtr, &len);
    const char *trimchars = default_trim_chars;
    int trimcharslen = default_trim_chars_len;
    const char *newstr;

    if (trimcharsObjPtr) {
        trimchars = Jim_GetString(trimcharsObjPtr, &trimcharslen);
    }

    newstr = JimFindTrimLeft(str, len, trimchars, trimcharslen);
    if (newstr == str) {
        return strObjPtr;
    }

    return Jim_NewStringObj(interp, newstr, len - (newstr - str));
}

static Jim_Obj *JimStringTrimRight(Jim_Interp *interp, Jim_Obj *strObjPtr, Jim_Obj *trimcharsObjPtr)
{
    int len;
    const char *trimchars = default_trim_chars;
    int trimcharslen = default_trim_chars_len;
    const char *nontrim;

    if (trimcharsObjPtr) {
        trimchars = Jim_GetString(trimcharsObjPtr, &trimcharslen);
    }

    SetStringFromAny(interp, strObjPtr);

    len = Jim_Length(strObjPtr);
    nontrim = JimFindTrimRight(strObjPtr->bytes, len, trimchars, trimcharslen);

    if (nontrim == NULL) {

        return Jim_NewEmptyStringObj(interp);
    }
    if (nontrim == strObjPtr->bytes + len) {

        return strObjPtr;
    }

    if (Jim_IsShared(strObjPtr)) {
        strObjPtr = Jim_NewStringObj(interp, strObjPtr->bytes, (nontrim - strObjPtr->bytes));
    }
    else {

        strObjPtr->bytes[nontrim - strObjPtr->bytes] = 0;
        strObjPtr->length = (nontrim - strObjPtr->bytes);
    }

    return strObjPtr;
}

static Jim_Obj *JimStringTrim(Jim_Interp *interp, Jim_Obj *strObjPtr, Jim_Obj *trimcharsObjPtr)
{

    Jim_Obj *objPtr = JimStringTrimLeft(interp, strObjPtr, trimcharsObjPtr);


    strObjPtr = JimStringTrimRight(interp, objPtr, trimcharsObjPtr);


    if (objPtr != strObjPtr && objPtr->refCount == 0) {

        Jim_FreeNewObj(interp, objPtr);
    }

    return strObjPtr;
}


#ifdef HAVE_ISASCII
#define jim_isascii isascii
#else
static int jim_isascii(int c)
{
    return !(c & ~0x7f);
}
#endif

static int JimStringIs(Jim_Interp *interp, Jim_Obj *strObjPtr, Jim_Obj *strClass, int strict)
{
    static const char * const strclassnames[] = {
        "integer", "alpha", "alnum", "ascii", "digit",
        "double", "lower", "upper", "space", "xdigit",
        "control", "print", "graph", "punct", "boolean",
        NULL
    };
    enum {
        STR_IS_INTEGER, STR_IS_ALPHA, STR_IS_ALNUM, STR_IS_ASCII, STR_IS_DIGIT,
        STR_IS_DOUBLE, STR_IS_LOWER, STR_IS_UPPER, STR_IS_SPACE, STR_IS_XDIGIT,
        STR_IS_CONTROL, STR_IS_PRINT, STR_IS_GRAPH, STR_IS_PUNCT, STR_IS_BOOLEAN,
    };
    int strclass;
    int len;
    int i;
    const char *str;
    int (*isclassfunc)(int c) = NULL;

    if (Jim_GetEnum(interp, strClass, strclassnames, &strclass, "class", JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
        return JIM_ERR;
    }

    str = Jim_GetString(strObjPtr, &len);
    if (len == 0) {
        Jim_SetResultBool(interp, !strict);
        return JIM_OK;
    }

    switch (strclass) {
        case STR_IS_INTEGER:
            {
                jim_wide w;
                Jim_SetResultBool(interp, JimGetWideNoErr(interp, strObjPtr, &w) == JIM_OK);
                return JIM_OK;
            }

        case STR_IS_DOUBLE:
            {
                double d;
                Jim_SetResultBool(interp, Jim_GetDouble(interp, strObjPtr, &d) == JIM_OK && errno != ERANGE);
                return JIM_OK;
            }

        case STR_IS_BOOLEAN:
            {
                int b;
                Jim_SetResultBool(interp, Jim_GetBoolean(interp, strObjPtr, &b) == JIM_OK);
                return JIM_OK;
            }

        case STR_IS_ALPHA: isclassfunc = isalpha; break;
        case STR_IS_ALNUM: isclassfunc = isalnum; break;
        case STR_IS_ASCII: isclassfunc = jim_isascii; break;
        case STR_IS_DIGIT: isclassfunc = isdigit; break;
        case STR_IS_LOWER: isclassfunc = islower; break;
        case STR_IS_UPPER: isclassfunc = isupper; break;
        case STR_IS_SPACE: isclassfunc = isspace; break;
        case STR_IS_XDIGIT: isclassfunc = isxdigit; break;
        case STR_IS_CONTROL: isclassfunc = iscntrl; break;
        case STR_IS_PRINT: isclassfunc = isprint; break;
        case STR_IS_GRAPH: isclassfunc = isgraph; break;
        case STR_IS_PUNCT: isclassfunc = ispunct; break;
        default:
            return JIM_ERR;
    }

    for (i = 0; i < len; i++) {
        if (!isclassfunc(UCHAR(str[i]))) {
            Jim_SetResultBool(interp, 0);
            return JIM_OK;
        }
    }
    Jim_SetResultBool(interp, 1);
    return JIM_OK;
}



static const Jim_ObjType comparedStringObjType = {
    "compared-string",
    NULL,
    NULL,
    NULL,
    JIM_TYPE_REFERENCES,
};

int Jim_CompareStringImmediate(Jim_Interp *interp, Jim_Obj *objPtr, const char *str)
{
    if (objPtr->typePtr == &comparedStringObjType && objPtr->internalRep.ptr == str) {
        return 1;
    }
    else {
        if (strcmp(str, Jim_String(objPtr)) != 0)
            return 0;

        if (objPtr->typePtr != &comparedStringObjType) {
            Jim_FreeIntRep(interp, objPtr);
            objPtr->typePtr = &comparedStringObjType;
        }
        objPtr->internalRep.ptr = (char *)str;
        return 1;
    }
}

static int qsortCompareStringPointers(const void *a, const void *b)
{
    char *const *sa = (char *const *)a;
    char *const *sb = (char *const *)b;

    return strcmp(*sa, *sb);
}



static void FreeSourceInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void DupSourceInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);

static const Jim_ObjType sourceObjType = {
    "source",
    FreeSourceInternalRep,
    DupSourceInternalRep,
    NULL,
    JIM_TYPE_REFERENCES,
};

void FreeSourceInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_DecrRefCount(interp, objPtr->internalRep.sourceValue.fileNameObj);
}

void DupSourceInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    dupPtr->internalRep.sourceValue = srcPtr->internalRep.sourceValue;
    Jim_IncrRefCount(dupPtr->internalRep.sourceValue.fileNameObj);
}

static void JimSetSourceInfo(Jim_Interp *interp, Jim_Obj *objPtr,
    Jim_Obj *fileNameObj, int lineNumber)
{
    JimPanic((Jim_IsShared(objPtr), "JimSetSourceInfo called with shared object"));
    JimPanic((objPtr->typePtr != NULL, "JimSetSourceInfo called with typed object"));
    Jim_IncrRefCount(fileNameObj);
    objPtr->internalRep.sourceValue.fileNameObj = fileNameObj;
    objPtr->internalRep.sourceValue.lineNumber = lineNumber;
    objPtr->typePtr = &sourceObjType;
}

static const Jim_ObjType scriptLineObjType = {
    "scriptline",
    NULL,
    NULL,
    NULL,
    JIM_NONE,
};

static Jim_Obj *JimNewScriptLineObj(Jim_Interp *interp, int argc, int line)
{
    Jim_Obj *objPtr;

#ifdef DEBUG_SHOW_SCRIPT
    char buf[100];
    snprintf(buf, sizeof(buf), "line=%d, argc=%d", line, argc);
    objPtr = Jim_NewStringObj(interp, buf, -1);
#else
    objPtr = Jim_NewEmptyStringObj(interp);
#endif
    objPtr->typePtr = &scriptLineObjType;
    objPtr->internalRep.scriptLineValue.argc = argc;
    objPtr->internalRep.scriptLineValue.line = line;

    return objPtr;
}

static void FreeScriptInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void DupScriptInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);

static const Jim_ObjType scriptObjType = {
    "script",
    FreeScriptInternalRep,
    DupScriptInternalRep,
    NULL,
    JIM_TYPE_REFERENCES,
};

typedef struct ScriptToken
{
    Jim_Obj *objPtr;
    int type;
} ScriptToken;

typedef struct ScriptObj
{
    ScriptToken *token;
    Jim_Obj *fileNameObj;
    int len;
    int substFlags;
    int inUse;                  /* Used to share a ScriptObj. Currently
                                   only used by Jim_EvalObj() as protection against
                                   shimmering of the currently evaluated object. */
    int firstline;
    int linenr;
    int missing;
} ScriptObj;

static void JimSetScriptFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr);
static int JimParseCheckMissing(Jim_Interp *interp, int ch);
static ScriptObj *JimGetScript(Jim_Interp *interp, Jim_Obj *objPtr);

void FreeScriptInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    int i;
    struct ScriptObj *script = (void *)objPtr->internalRep.ptr;

    if (--script->inUse != 0)
        return;
    for (i = 0; i < script->len; i++) {
        Jim_DecrRefCount(interp, script->token[i].objPtr);
    }
    Jim_Free(script->token);
    Jim_DecrRefCount(interp, script->fileNameObj);
    Jim_Free(script);
}

void DupScriptInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    JIM_NOTUSED(interp);
    JIM_NOTUSED(srcPtr);

    dupPtr->typePtr = NULL;
}

typedef struct
{
    const char *token;
    int len;
    int type;
    int line;
} ParseToken;

typedef struct
{

    ParseToken *list;
    int size;
    int count;
    ParseToken static_list[20];
} ParseTokenList;

static void ScriptTokenListInit(ParseTokenList *tokenlist)
{
    tokenlist->list = tokenlist->static_list;
    tokenlist->size = sizeof(tokenlist->static_list) / sizeof(ParseToken);
    tokenlist->count = 0;
}

static void ScriptTokenListFree(ParseTokenList *tokenlist)
{
    if (tokenlist->list != tokenlist->static_list) {
        Jim_Free(tokenlist->list);
    }
}

static void ScriptAddToken(ParseTokenList *tokenlist, const char *token, int len, int type,
    int line)
{
    ParseToken *t;

    if (tokenlist->count == tokenlist->size) {

        tokenlist->size *= 2;
        if (tokenlist->list != tokenlist->static_list) {
            tokenlist->list =
                Jim_Realloc(tokenlist->list, tokenlist->size * sizeof(*tokenlist->list));
        }
        else {

            tokenlist->list = Jim_Alloc(tokenlist->size * sizeof(*tokenlist->list));
            memcpy(tokenlist->list, tokenlist->static_list,
                tokenlist->count * sizeof(*tokenlist->list));
        }
    }
    t = &tokenlist->list[tokenlist->count++];
    t->token = token;
    t->len = len;
    t->type = type;
    t->line = line;
}

static int JimCountWordTokens(struct ScriptObj *script, ParseToken *t)
{
    int expand = 1;
    int count = 0;


    if (t->type == JIM_TT_STR && !TOKEN_IS_SEP(t[1].type)) {
        if ((t->len == 1 && *t->token == '*') || (t->len == 6 && strncmp(t->token, "expand", 6) == 0)) {

            expand = -1;
            t++;
        }
        else {
            if (script->missing == ' ') {

                script->missing = '}';
                script->linenr = t[1].line;
            }
        }
    }


    while (!TOKEN_IS_SEP(t->type)) {
        t++;
        count++;
    }

    return count * expand;
}

static Jim_Obj *JimMakeScriptObj(Jim_Interp *interp, const ParseToken *t)
{
    Jim_Obj *objPtr;

    if (t->type == JIM_TT_ESC && memchr(t->token, '\\', t->len) != NULL) {

        int len = t->len;
        char *str = Jim_Alloc(len + 1);
        len = JimEscape(str, t->token, len);
        objPtr = Jim_NewStringObjNoAlloc(interp, str, len);
    }
    else {
        objPtr = Jim_NewStringObj(interp, t->token, t->len);
    }
    return objPtr;
}

static void ScriptObjAddTokens(Jim_Interp *interp, struct ScriptObj *script,
    ParseTokenList *tokenlist)
{
    int i;
    struct ScriptToken *token;

    int lineargs = 0;

    ScriptToken *linefirst;
    int count;
    int linenr;

#ifdef DEBUG_SHOW_SCRIPT_TOKENS
    printf("==== Tokens ====\n");
    for (i = 0; i < tokenlist->count; i++) {
        printf("[%2d]@%d %s '%.*s'\n", i, tokenlist->list[i].line, jim_tt_name(tokenlist->list[i].type),
            tokenlist->list[i].len, tokenlist->list[i].token);
    }
#endif


    count = tokenlist->count;
    for (i = 0; i < tokenlist->count; i++) {
        if (tokenlist->list[i].type == JIM_TT_EOL) {
            count++;
        }
    }
    linenr = script->firstline = tokenlist->list[0].line;

    token = script->token = Jim_Alloc(sizeof(ScriptToken) * count);


    linefirst = token++;

    for (i = 0; i < tokenlist->count; ) {

        int wordtokens;


        while (tokenlist->list[i].type == JIM_TT_SEP) {
            i++;
        }

        wordtokens = JimCountWordTokens(script, tokenlist->list + i);

        if (wordtokens == 0) {

            if (lineargs) {
                linefirst->type = JIM_TT_LINE;
                linefirst->objPtr = JimNewScriptLineObj(interp, lineargs, linenr);
                Jim_IncrRefCount(linefirst->objPtr);


                lineargs = 0;
                linefirst = token++;
            }
            i++;
            continue;
        }
        else if (wordtokens != 1) {

            token->type = JIM_TT_WORD;
            token->objPtr = Jim_NewIntObj(interp, wordtokens);
            Jim_IncrRefCount(token->objPtr);
            token++;
            if (wordtokens < 0) {

                i++;
                wordtokens = -wordtokens - 1;
                lineargs--;
            }
        }

        if (lineargs == 0) {

            linenr = tokenlist->list[i].line;
        }
        lineargs++;


        while (wordtokens--) {
            const ParseToken *t = &tokenlist->list[i++];

            token->type = t->type;
            token->objPtr = JimMakeScriptObj(interp, t);
            Jim_IncrRefCount(token->objPtr);

            JimSetSourceInfo(interp, token->objPtr, script->fileNameObj, t->line);
            token++;
        }
    }

    if (lineargs == 0) {
        token--;
    }

    script->len = token - script->token;

    JimPanic((script->len >= count, "allocated script array is too short"));

#ifdef DEBUG_SHOW_SCRIPT
    printf("==== Script (%s) ====\n", Jim_String(script->fileNameObj));
    for (i = 0; i < script->len; i++) {
        const ScriptToken *t = &script->token[i];
        printf("[%2d] %s %s\n", i, jim_tt_name(t->type), Jim_String(t->objPtr));
    }
#endif

}

int Jim_ScriptIsComplete(Jim_Interp *interp, Jim_Obj *scriptObj, char *stateCharPtr)
{
    ScriptObj *script = JimGetScript(interp, scriptObj);
    if (stateCharPtr) {
        *stateCharPtr = script->missing;
    }
    return script->missing == ' ' || script->missing == '}';
}

static int JimParseCheckMissing(Jim_Interp *interp, int ch)
{
    const char *msg;

    switch (ch) {
        case '\\':
        case ' ':
            return JIM_OK;

        case '[':
            msg = "unmatched \"[\"";
            break;
        case '{':
            msg = "missing close-brace";
            break;
        case '}':
            msg = "extra characters after close-brace";
            break;
        case '"':
        default:
            msg = "missing quote";
            break;
    }

    Jim_SetResultString(interp, msg, -1);
    return JIM_ERR;
}

static void SubstObjAddTokens(Jim_Interp *interp, struct ScriptObj *script,
    ParseTokenList *tokenlist)
{
    int i;
    struct ScriptToken *token;

    token = script->token = Jim_Alloc(sizeof(ScriptToken) * tokenlist->count);

    for (i = 0; i < tokenlist->count; i++) {
        const ParseToken *t = &tokenlist->list[i];


        token->type = t->type;
        token->objPtr = JimMakeScriptObj(interp, t);
        Jim_IncrRefCount(token->objPtr);
        token++;
    }

    script->len = i;
}

static void JimSetScriptFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    int scriptTextLen;
    const char *scriptText = Jim_GetString(objPtr, &scriptTextLen);
    struct JimParserCtx parser;
    struct ScriptObj *script;
    ParseTokenList tokenlist;
    int line = 1;


    if (objPtr->typePtr == &sourceObjType) {
        line = objPtr->internalRep.sourceValue.lineNumber;
    }


    ScriptTokenListInit(&tokenlist);

    JimParserInit(&parser, scriptText, scriptTextLen, line);
    while (!parser.eof) {
        JimParseScript(&parser);
        ScriptAddToken(&tokenlist, parser.tstart, parser.tend - parser.tstart + 1, parser.tt,
            parser.tline);
    }


    ScriptAddToken(&tokenlist, scriptText + scriptTextLen, 0, JIM_TT_EOF, 0);


    script = Jim_Alloc(sizeof(*script));
    memset(script, 0, sizeof(*script));
    script->inUse = 1;
    if (objPtr->typePtr == &sourceObjType) {
        script->fileNameObj = objPtr->internalRep.sourceValue.fileNameObj;
    }
    else {
        script->fileNameObj = interp->emptyObj;
    }
    Jim_IncrRefCount(script->fileNameObj);
    script->missing = parser.missing.ch;
    script->linenr = parser.missing.line;

    ScriptObjAddTokens(interp, script, &tokenlist);


    ScriptTokenListFree(&tokenlist);


    Jim_FreeIntRep(interp, objPtr);
    Jim_SetIntRepPtr(objPtr, script);
    objPtr->typePtr = &scriptObjType;
}

static void JimAddErrorToStack(Jim_Interp *interp, ScriptObj *script);

static ScriptObj *JimGetScript(Jim_Interp *interp, Jim_Obj *objPtr)
{
    if (objPtr == interp->emptyObj) {

        objPtr = interp->nullScriptObj;
    }

    if (objPtr->typePtr != &scriptObjType || ((struct ScriptObj *)Jim_GetIntRepPtr(objPtr))->substFlags) {
        JimSetScriptFromAny(interp, objPtr);
    }

    return (ScriptObj *)Jim_GetIntRepPtr(objPtr);
}

static int JimScriptValid(Jim_Interp *interp, ScriptObj *script)
{
    if (JimParseCheckMissing(interp, script->missing) == JIM_ERR) {
        JimAddErrorToStack(interp, script);
        return 0;
    }
    return 1;
}


static void JimIncrCmdRefCount(Jim_Cmd *cmdPtr)
{
    cmdPtr->inUse++;
}

static void JimDecrCmdRefCount(Jim_Interp *interp, Jim_Cmd *cmdPtr)
{
    if (--cmdPtr->inUse == 0) {
        if (cmdPtr->isproc) {
            Jim_DecrRefCount(interp, cmdPtr->u.proc.argListObjPtr);
            Jim_DecrRefCount(interp, cmdPtr->u.proc.bodyObjPtr);
            Jim_DecrRefCount(interp, cmdPtr->u.proc.nsObj);
            if (cmdPtr->u.proc.staticVars) {
                Jim_FreeHashTable(cmdPtr->u.proc.staticVars);
                Jim_Free(cmdPtr->u.proc.staticVars);
            }
        }
        else {

            if (cmdPtr->u.native.delProc) {
                cmdPtr->u.native.delProc(interp, cmdPtr->u.native.privData);
            }
        }
        if (cmdPtr->prevCmd) {

            JimDecrCmdRefCount(interp, cmdPtr->prevCmd);
        }
        Jim_Free(cmdPtr);
    }
}

static void JimVariablesHTValDestructor(void *interp, void *val)
{
    Jim_DecrRefCount(interp, ((Jim_Var *)val)->objPtr);
    Jim_Free(val);
}

static const Jim_HashTableType JimVariablesHashTableType = {
    JimStringCopyHTHashFunction,
    JimStringCopyHTDup,
    NULL,
    JimStringCopyHTKeyCompare,
    JimStringCopyHTKeyDestructor,
    JimVariablesHTValDestructor
};

static void JimCommandsHT_ValDestructor(void *interp, void *val)
{
    JimDecrCmdRefCount(interp, val);
}

static const Jim_HashTableType JimCommandsHashTableType = {
    JimStringCopyHTHashFunction,
    JimStringCopyHTDup,
    NULL,
    JimStringCopyHTKeyCompare,
    JimStringCopyHTKeyDestructor,
    JimCommandsHT_ValDestructor
};



#ifdef jim_ext_namespace
static Jim_Obj *JimQualifyNameObj(Jim_Interp *interp, Jim_Obj *nsObj)
{
    const char *name = Jim_String(nsObj);
    if (name[0] == ':' && name[1] == ':') {

        while (*++name == ':') {
        }
        nsObj = Jim_NewStringObj(interp, name, -1);
    }
    else if (Jim_Length(interp->framePtr->nsObj)) {

        nsObj = Jim_DuplicateObj(interp, interp->framePtr->nsObj);
        Jim_AppendStrings(interp, nsObj, "::", name, NULL);
    }
    return nsObj;
}

Jim_Obj *Jim_MakeGlobalNamespaceName(Jim_Interp *interp, Jim_Obj *nameObjPtr)
{
    Jim_Obj *resultObj;

    const char *name = Jim_String(nameObjPtr);
    if (name[0] == ':' && name[1] == ':') {
        return nameObjPtr;
    }
    Jim_IncrRefCount(nameObjPtr);
    resultObj = Jim_NewStringObj(interp, "::", -1);
    Jim_AppendObj(interp, resultObj, nameObjPtr);
    Jim_DecrRefCount(interp, nameObjPtr);

    return resultObj;
}

static const char *JimQualifyName(Jim_Interp *interp, const char *name, Jim_Obj **objPtrPtr)
{
    Jim_Obj *objPtr = interp->emptyObj;

    if (name[0] == ':' && name[1] == ':') {

        while (*++name == ':') {
        }
    }
    else if (Jim_Length(interp->framePtr->nsObj)) {

        objPtr = Jim_DuplicateObj(interp, interp->framePtr->nsObj);
        Jim_AppendStrings(interp, objPtr, "::", name, NULL);
        name = Jim_String(objPtr);
    }
    Jim_IncrRefCount(objPtr);
    *objPtrPtr = objPtr;
    return name;
}

    #define JimFreeQualifiedName(INTERP, OBJ) Jim_DecrRefCount((INTERP), (OBJ))

#else

    #define JimQualifyName(INTERP, NAME, DUMMY) (((NAME)[0] == ':' && (NAME)[1] == ':') ? (NAME) + 2 : (NAME))
    #define JimFreeQualifiedName(INTERP, DUMMY) (void)(DUMMY)

Jim_Obj *Jim_MakeGlobalNamespaceName(Jim_Interp *interp, Jim_Obj *nameObjPtr)
{
    return nameObjPtr;
}
#endif

static int JimCreateCommand(Jim_Interp *interp, const char *name, Jim_Cmd *cmd)
{
    Jim_HashEntry *he = Jim_FindHashEntry(&interp->commands, name);
    if (he) {

        Jim_InterpIncrProcEpoch(interp);
    }

    if (he && interp->local) {

        cmd->prevCmd = Jim_GetHashEntryVal(he);
        Jim_SetHashVal(&interp->commands, he, cmd);
    }
    else {
        if (he) {

            Jim_DeleteHashEntry(&interp->commands, name);
        }

        Jim_AddHashEntry(&interp->commands, name, cmd);
    }
    return JIM_OK;
}


int Jim_CreateCommand(Jim_Interp *interp, const char *cmdNameStr,
    Jim_CmdProc *cmdProc, void *privData, Jim_DelCmdProc *delProc)
{
    Jim_Cmd *cmdPtr = Jim_Alloc(sizeof(*cmdPtr));


    memset(cmdPtr, 0, sizeof(*cmdPtr));
    cmdPtr->inUse = 1;
    cmdPtr->u.native.delProc = delProc;
    cmdPtr->u.native.cmdProc = cmdProc;
    cmdPtr->u.native.privData = privData;

    JimCreateCommand(interp, cmdNameStr, cmdPtr);

    return JIM_OK;
}

static int JimCreateProcedureStatics(Jim_Interp *interp, Jim_Cmd *cmdPtr, Jim_Obj *staticsListObjPtr)
{
    int len, i;

    len = Jim_ListLength(interp, staticsListObjPtr);
    if (len == 0) {
        return JIM_OK;
    }

    cmdPtr->u.proc.staticVars = Jim_Alloc(sizeof(Jim_HashTable));
    Jim_InitHashTable(cmdPtr->u.proc.staticVars, &JimVariablesHashTableType, interp);
    for (i = 0; i < len; i++) {
        Jim_Obj *objPtr, *initObjPtr, *nameObjPtr;
        Jim_Var *varPtr;
        int subLen;

        objPtr = Jim_ListGetIndex(interp, staticsListObjPtr, i);

        subLen = Jim_ListLength(interp, objPtr);
        if (subLen == 1 || subLen == 2) {
            nameObjPtr = Jim_ListGetIndex(interp, objPtr, 0);
            if (subLen == 1) {
                initObjPtr = Jim_GetVariable(interp, nameObjPtr, JIM_NONE);
                if (initObjPtr == NULL) {
                    Jim_SetResultFormatted(interp,
                        "variable for initialization of static \"%#s\" not found in the local context",
                        nameObjPtr);
                    return JIM_ERR;
                }
            }
            else {
                initObjPtr = Jim_ListGetIndex(interp, objPtr, 1);
            }
            if (JimValidName(interp, "static variable", nameObjPtr) != JIM_OK) {
                return JIM_ERR;
            }

            varPtr = Jim_Alloc(sizeof(*varPtr));
            varPtr->objPtr = initObjPtr;
            Jim_IncrRefCount(initObjPtr);
            varPtr->linkFramePtr = NULL;
            if (Jim_AddHashEntry(cmdPtr->u.proc.staticVars,
                Jim_String(nameObjPtr), varPtr) != JIM_OK) {
                Jim_SetResultFormatted(interp,
                    "static variable name \"%#s\" duplicated in statics list", nameObjPtr);
                Jim_DecrRefCount(interp, initObjPtr);
                Jim_Free(varPtr);
                return JIM_ERR;
            }
        }
        else {
            Jim_SetResultFormatted(interp, "too many fields in static specifier \"%#s\"",
                objPtr);
            return JIM_ERR;
        }
    }
    return JIM_OK;
}

static void JimUpdateProcNamespace(Jim_Interp *interp, Jim_Cmd *cmdPtr, const char *cmdname)
{
#ifdef jim_ext_namespace
    if (cmdPtr->isproc) {

        const char *pt = strrchr(cmdname, ':');
        if (pt && pt != cmdname && pt[-1] == ':') {
            Jim_DecrRefCount(interp, cmdPtr->u.proc.nsObj);
            cmdPtr->u.proc.nsObj = Jim_NewStringObj(interp, cmdname, pt - cmdname - 1);
            Jim_IncrRefCount(cmdPtr->u.proc.nsObj);

            if (Jim_FindHashEntry(&interp->commands, pt + 1)) {

                Jim_InterpIncrProcEpoch(interp);
            }
        }
    }
#endif
}

static Jim_Cmd *JimCreateProcedureCmd(Jim_Interp *interp, Jim_Obj *argListObjPtr,
    Jim_Obj *staticsListObjPtr, Jim_Obj *bodyObjPtr, Jim_Obj *nsObj)
{
    Jim_Cmd *cmdPtr;
    int argListLen;
    int i;

    argListLen = Jim_ListLength(interp, argListObjPtr);


    cmdPtr = Jim_Alloc(sizeof(*cmdPtr) + sizeof(struct Jim_ProcArg) * argListLen);
    memset(cmdPtr, 0, sizeof(*cmdPtr));
    cmdPtr->inUse = 1;
    cmdPtr->isproc = 1;
    cmdPtr->u.proc.argListObjPtr = argListObjPtr;
    cmdPtr->u.proc.argListLen = argListLen;
    cmdPtr->u.proc.bodyObjPtr = bodyObjPtr;
    cmdPtr->u.proc.argsPos = -1;
    cmdPtr->u.proc.arglist = (struct Jim_ProcArg *)(cmdPtr + 1);
    cmdPtr->u.proc.nsObj = nsObj ? nsObj : interp->emptyObj;
    Jim_IncrRefCount(argListObjPtr);
    Jim_IncrRefCount(bodyObjPtr);
    Jim_IncrRefCount(cmdPtr->u.proc.nsObj);


    if (staticsListObjPtr && JimCreateProcedureStatics(interp, cmdPtr, staticsListObjPtr) != JIM_OK) {
        goto err;
    }



    for (i = 0; i < argListLen; i++) {
        Jim_Obj *argPtr;
        Jim_Obj *nameObjPtr;
        Jim_Obj *defaultObjPtr;
        int len;


        argPtr = Jim_ListGetIndex(interp, argListObjPtr, i);
        len = Jim_ListLength(interp, argPtr);
        if (len == 0) {
            Jim_SetResultString(interp, "argument with no name", -1);
err:
            JimDecrCmdRefCount(interp, cmdPtr);
            return NULL;
        }
        if (len > 2) {
            Jim_SetResultFormatted(interp, "too many fields in argument specifier \"%#s\"", argPtr);
            goto err;
        }

        if (len == 2) {

            nameObjPtr = Jim_ListGetIndex(interp, argPtr, 0);
            defaultObjPtr = Jim_ListGetIndex(interp, argPtr, 1);
        }
        else {

            nameObjPtr = argPtr;
            defaultObjPtr = NULL;
        }


        if (Jim_CompareStringImmediate(interp, nameObjPtr, "args")) {
            if (cmdPtr->u.proc.argsPos >= 0) {
                Jim_SetResultString(interp, "'args' specified more than once", -1);
                goto err;
            }
            cmdPtr->u.proc.argsPos = i;
        }
        else {
            if (len == 2) {
                cmdPtr->u.proc.optArity++;
            }
            else {
                cmdPtr->u.proc.reqArity++;
            }
        }

        cmdPtr->u.proc.arglist[i].nameObjPtr = nameObjPtr;
        cmdPtr->u.proc.arglist[i].defaultObjPtr = defaultObjPtr;
    }

    return cmdPtr;
}

int Jim_DeleteCommand(Jim_Interp *interp, const char *name)
{
    int ret = JIM_OK;
    Jim_Obj *qualifiedNameObj;
    const char *qualname = JimQualifyName(interp, name, &qualifiedNameObj);

    if (Jim_DeleteHashEntry(&interp->commands, qualname) == JIM_ERR) {
        Jim_SetResultFormatted(interp, "can't delete \"%s\": command doesn't exist", name);
        ret = JIM_ERR;
    }
    else {
        Jim_InterpIncrProcEpoch(interp);
    }

    JimFreeQualifiedName(interp, qualifiedNameObj);

    return ret;
}

int Jim_RenameCommand(Jim_Interp *interp, const char *oldName, const char *newName)
{
    int ret = JIM_ERR;
    Jim_HashEntry *he;
    Jim_Cmd *cmdPtr;
    Jim_Obj *qualifiedOldNameObj;
    Jim_Obj *qualifiedNewNameObj;
    const char *fqold;
    const char *fqnew;

    if (newName[0] == 0) {
        return Jim_DeleteCommand(interp, oldName);
    }

    fqold = JimQualifyName(interp, oldName, &qualifiedOldNameObj);
    fqnew = JimQualifyName(interp, newName, &qualifiedNewNameObj);


    he = Jim_FindHashEntry(&interp->commands, fqold);
    if (he == NULL) {
        Jim_SetResultFormatted(interp, "can't rename \"%s\": command doesn't exist", oldName);
    }
    else if (Jim_FindHashEntry(&interp->commands, fqnew)) {
        Jim_SetResultFormatted(interp, "can't rename to \"%s\": command already exists", newName);
    }
    else {

        cmdPtr = Jim_GetHashEntryVal(he);
        JimIncrCmdRefCount(cmdPtr);
        JimUpdateProcNamespace(interp, cmdPtr, fqnew);
        Jim_AddHashEntry(&interp->commands, fqnew, cmdPtr);


        Jim_DeleteHashEntry(&interp->commands, fqold);


        Jim_InterpIncrProcEpoch(interp);

        ret = JIM_OK;
    }

    JimFreeQualifiedName(interp, qualifiedOldNameObj);
    JimFreeQualifiedName(interp, qualifiedNewNameObj);

    return ret;
}


static void FreeCommandInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_DecrRefCount(interp, objPtr->internalRep.cmdValue.nsObj);
}

static void DupCommandInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    dupPtr->internalRep.cmdValue = srcPtr->internalRep.cmdValue;
    dupPtr->typePtr = srcPtr->typePtr;
    Jim_IncrRefCount(dupPtr->internalRep.cmdValue.nsObj);
}

static const Jim_ObjType commandObjType = {
    "command",
    FreeCommandInternalRep,
    DupCommandInternalRep,
    NULL,
    JIM_TYPE_REFERENCES,
};

Jim_Cmd *Jim_GetCommand(Jim_Interp *interp, Jim_Obj *objPtr, int flags)
{
    Jim_Cmd *cmd;

    if (objPtr->typePtr != &commandObjType ||
            objPtr->internalRep.cmdValue.procEpoch != interp->procEpoch
#ifdef jim_ext_namespace
            || !Jim_StringEqObj(objPtr->internalRep.cmdValue.nsObj, interp->framePtr->nsObj)
#endif
        ) {



        const char *name = Jim_String(objPtr);
        Jim_HashEntry *he;

        if (name[0] == ':' && name[1] == ':') {
            while (*++name == ':') {
            }
        }
#ifdef jim_ext_namespace
        else if (Jim_Length(interp->framePtr->nsObj)) {

            Jim_Obj *nameObj = Jim_DuplicateObj(interp, interp->framePtr->nsObj);
            Jim_AppendStrings(interp, nameObj, "::", name, NULL);
            he = Jim_FindHashEntry(&interp->commands, Jim_String(nameObj));
            Jim_FreeNewObj(interp, nameObj);
            if (he) {
                goto found;
            }
        }
#endif


        he = Jim_FindHashEntry(&interp->commands, name);
        if (he == NULL) {
            if (flags & JIM_ERRMSG) {
                Jim_SetResultFormatted(interp, "invalid command name \"%#s\"", objPtr);
            }
            return NULL;
        }
#ifdef jim_ext_namespace
found:
#endif
        cmd = Jim_GetHashEntryVal(he);


        Jim_FreeIntRep(interp, objPtr);
        objPtr->typePtr = &commandObjType;
        objPtr->internalRep.cmdValue.procEpoch = interp->procEpoch;
        objPtr->internalRep.cmdValue.cmdPtr = cmd;
        objPtr->internalRep.cmdValue.nsObj = interp->framePtr->nsObj;
        Jim_IncrRefCount(interp->framePtr->nsObj);
    }
    else {
        cmd = objPtr->internalRep.cmdValue.cmdPtr;
    }
    while (cmd->u.proc.upcall) {
        cmd = cmd->prevCmd;
    }
    return cmd;
}



#define JIM_DICT_SUGAR 100

static int SetVariableFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr);

static const Jim_ObjType variableObjType = {
    "variable",
    NULL,
    NULL,
    NULL,
    JIM_TYPE_REFERENCES,
};

static int JimValidName(Jim_Interp *interp, const char *type, Jim_Obj *nameObjPtr)
{

    if (nameObjPtr->typePtr != &variableObjType) {
        int len;
        const char *str = Jim_GetString(nameObjPtr, &len);
        if (memchr(str, '\0', len)) {
            Jim_SetResultFormatted(interp, "%s name contains embedded null", type);
            return JIM_ERR;
        }
    }
    return JIM_OK;
}

static int SetVariableFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    const char *varName;
    Jim_CallFrame *framePtr;
    Jim_HashEntry *he;
    int global;
    int len;


    if (objPtr->typePtr == &variableObjType) {
        framePtr = objPtr->internalRep.varValue.global ? interp->topFramePtr : interp->framePtr;
        if (objPtr->internalRep.varValue.callFrameId == framePtr->id) {

            return JIM_OK;
        }

    }
    else if (objPtr->typePtr == &dictSubstObjType) {
        return JIM_DICT_SUGAR;
    }
    else if (JimValidName(interp, "variable", objPtr) != JIM_OK) {
        return JIM_ERR;
    }


    varName = Jim_GetString(objPtr, &len);


    if (len && varName[len - 1] == ')' && strchr(varName, '(') != NULL) {
        return JIM_DICT_SUGAR;
    }

    if (varName[0] == ':' && varName[1] == ':') {
        while (*++varName == ':') {
        }
        global = 1;
        framePtr = interp->topFramePtr;
    }
    else {
        global = 0;
        framePtr = interp->framePtr;
    }


    he = Jim_FindHashEntry(&framePtr->vars, varName);
    if (he == NULL) {
        if (!global && framePtr->staticVars) {

            he = Jim_FindHashEntry(framePtr->staticVars, varName);
        }
        if (he == NULL) {
            return JIM_ERR;
        }
    }


    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &variableObjType;
    objPtr->internalRep.varValue.callFrameId = framePtr->id;
    objPtr->internalRep.varValue.varPtr = Jim_GetHashEntryVal(he);
    objPtr->internalRep.varValue.global = global;
    return JIM_OK;
}


static int JimDictSugarSet(Jim_Interp *interp, Jim_Obj *ObjPtr, Jim_Obj *valObjPtr);
static Jim_Obj *JimDictSugarGet(Jim_Interp *interp, Jim_Obj *ObjPtr, int flags);

static Jim_Var *JimCreateVariable(Jim_Interp *interp, Jim_Obj *nameObjPtr, Jim_Obj *valObjPtr)
{
    const char *name;
    Jim_CallFrame *framePtr;
    int global;


    Jim_Var *var = Jim_Alloc(sizeof(*var));

    var->objPtr = valObjPtr;
    Jim_IncrRefCount(valObjPtr);
    var->linkFramePtr = NULL;

    name = Jim_String(nameObjPtr);
    if (name[0] == ':' && name[1] == ':') {
        while (*++name == ':') {
        }
        framePtr = interp->topFramePtr;
        global = 1;
    }
    else {
        framePtr = interp->framePtr;
        global = 0;
    }


    Jim_AddHashEntry(&framePtr->vars, name, var);


    Jim_FreeIntRep(interp, nameObjPtr);
    nameObjPtr->typePtr = &variableObjType;
    nameObjPtr->internalRep.varValue.callFrameId = framePtr->id;
    nameObjPtr->internalRep.varValue.varPtr = var;
    nameObjPtr->internalRep.varValue.global = global;

    return var;
}


int Jim_SetVariable(Jim_Interp *interp, Jim_Obj *nameObjPtr, Jim_Obj *valObjPtr)
{
    int err;
    Jim_Var *var;

    switch (SetVariableFromAny(interp, nameObjPtr)) {
        case JIM_DICT_SUGAR:
            return JimDictSugarSet(interp, nameObjPtr, valObjPtr);

        case JIM_ERR:
            if (JimValidName(interp, "variable", nameObjPtr) != JIM_OK) {
                return JIM_ERR;
            }
            JimCreateVariable(interp, nameObjPtr, valObjPtr);
            break;

        case JIM_OK:
            var = nameObjPtr->internalRep.varValue.varPtr;
            if (var->linkFramePtr == NULL) {
                Jim_IncrRefCount(valObjPtr);
                Jim_DecrRefCount(interp, var->objPtr);
                var->objPtr = valObjPtr;
            }
            else {
                Jim_CallFrame *savedCallFrame;

                savedCallFrame = interp->framePtr;
                interp->framePtr = var->linkFramePtr;
                err = Jim_SetVariable(interp, var->objPtr, valObjPtr);
                interp->framePtr = savedCallFrame;
                if (err != JIM_OK)
                    return err;
            }
    }
    return JIM_OK;
}

int Jim_SetVariableStr(Jim_Interp *interp, const char *name, Jim_Obj *objPtr)
{
    Jim_Obj *nameObjPtr;
    int result;

    nameObjPtr = Jim_NewStringObj(interp, name, -1);
    Jim_IncrRefCount(nameObjPtr);
    result = Jim_SetVariable(interp, nameObjPtr, objPtr);
    Jim_DecrRefCount(interp, nameObjPtr);
    return result;
}

int Jim_SetGlobalVariableStr(Jim_Interp *interp, const char *name, Jim_Obj *objPtr)
{
    Jim_CallFrame *savedFramePtr;
    int result;

    savedFramePtr = interp->framePtr;
    interp->framePtr = interp->topFramePtr;
    result = Jim_SetVariableStr(interp, name, objPtr);
    interp->framePtr = savedFramePtr;
    return result;
}

int Jim_SetVariableStrWithStr(Jim_Interp *interp, const char *name, const char *val)
{
    Jim_Obj *valObjPtr;
    int result;

    valObjPtr = Jim_NewStringObj(interp, val, -1);
    Jim_IncrRefCount(valObjPtr);
    result = Jim_SetVariableStr(interp, name, valObjPtr);
    Jim_DecrRefCount(interp, valObjPtr);
    return result;
}

int Jim_SetVariableLink(Jim_Interp *interp, Jim_Obj *nameObjPtr,
    Jim_Obj *targetNameObjPtr, Jim_CallFrame *targetCallFrame)
{
    const char *varName;
    const char *targetName;
    Jim_CallFrame *framePtr;
    Jim_Var *varPtr;


    switch (SetVariableFromAny(interp, nameObjPtr)) {
        case JIM_DICT_SUGAR:

            Jim_SetResultFormatted(interp, "bad variable name \"%#s\": upvar won't create a scalar variable that looks like an array element", nameObjPtr);
            return JIM_ERR;

        case JIM_OK:
            varPtr = nameObjPtr->internalRep.varValue.varPtr;

            if (varPtr->linkFramePtr == NULL) {
                Jim_SetResultFormatted(interp, "variable \"%#s\" already exists", nameObjPtr);
                return JIM_ERR;
            }


            varPtr->linkFramePtr = NULL;
            break;
    }



    varName = Jim_String(nameObjPtr);

    if (varName[0] == ':' && varName[1] == ':') {
        while (*++varName == ':') {
        }

        framePtr = interp->topFramePtr;
    }
    else {
        framePtr = interp->framePtr;
    }

    targetName = Jim_String(targetNameObjPtr);
    if (targetName[0] == ':' && targetName[1] == ':') {
        while (*++targetName == ':') {
        }
        targetNameObjPtr = Jim_NewStringObj(interp, targetName, -1);
        targetCallFrame = interp->topFramePtr;
    }
    Jim_IncrRefCount(targetNameObjPtr);

    if (framePtr->level < targetCallFrame->level) {
        Jim_SetResultFormatted(interp,
            "bad variable name \"%#s\": upvar won't create namespace variable that refers to procedure variable",
            nameObjPtr);
        Jim_DecrRefCount(interp, targetNameObjPtr);
        return JIM_ERR;
    }


    if (framePtr == targetCallFrame) {
        Jim_Obj *objPtr = targetNameObjPtr;


        while (1) {
            if (strcmp(Jim_String(objPtr), varName) == 0) {
                Jim_SetResultString(interp, "can't upvar from variable to itself", -1);
                Jim_DecrRefCount(interp, targetNameObjPtr);
                return JIM_ERR;
            }
            if (SetVariableFromAny(interp, objPtr) != JIM_OK)
                break;
            varPtr = objPtr->internalRep.varValue.varPtr;
            if (varPtr->linkFramePtr != targetCallFrame)
                break;
            objPtr = varPtr->objPtr;
        }
    }


    Jim_SetVariable(interp, nameObjPtr, targetNameObjPtr);

    nameObjPtr->internalRep.varValue.varPtr->linkFramePtr = targetCallFrame;
    Jim_DecrRefCount(interp, targetNameObjPtr);
    return JIM_OK;
}

Jim_Obj *Jim_GetVariable(Jim_Interp *interp, Jim_Obj *nameObjPtr, int flags)
{
    switch (SetVariableFromAny(interp, nameObjPtr)) {
        case JIM_OK:{
                Jim_Var *varPtr = nameObjPtr->internalRep.varValue.varPtr;

                if (varPtr->linkFramePtr == NULL) {
                    return varPtr->objPtr;
                }
                else {
                    Jim_Obj *objPtr;


                    Jim_CallFrame *savedCallFrame = interp->framePtr;

                    interp->framePtr = varPtr->linkFramePtr;
                    objPtr = Jim_GetVariable(interp, varPtr->objPtr, flags);
                    interp->framePtr = savedCallFrame;
                    if (objPtr) {
                        return objPtr;
                    }

                }
            }
            break;

        case JIM_DICT_SUGAR:

            return JimDictSugarGet(interp, nameObjPtr, flags);
    }
    if (flags & JIM_ERRMSG) {
        Jim_SetResultFormatted(interp, "can't read \"%#s\": no such variable", nameObjPtr);
    }
    return NULL;
}

Jim_Obj *Jim_GetGlobalVariable(Jim_Interp *interp, Jim_Obj *nameObjPtr, int flags)
{
    Jim_CallFrame *savedFramePtr;
    Jim_Obj *objPtr;

    savedFramePtr = interp->framePtr;
    interp->framePtr = interp->topFramePtr;
    objPtr = Jim_GetVariable(interp, nameObjPtr, flags);
    interp->framePtr = savedFramePtr;

    return objPtr;
}

Jim_Obj *Jim_GetVariableStr(Jim_Interp *interp, const char *name, int flags)
{
    Jim_Obj *nameObjPtr, *varObjPtr;

    nameObjPtr = Jim_NewStringObj(interp, name, -1);
    Jim_IncrRefCount(nameObjPtr);
    varObjPtr = Jim_GetVariable(interp, nameObjPtr, flags);
    Jim_DecrRefCount(interp, nameObjPtr);
    return varObjPtr;
}

Jim_Obj *Jim_GetGlobalVariableStr(Jim_Interp *interp, const char *name, int flags)
{
    Jim_CallFrame *savedFramePtr;
    Jim_Obj *objPtr;

    savedFramePtr = interp->framePtr;
    interp->framePtr = interp->topFramePtr;
    objPtr = Jim_GetVariableStr(interp, name, flags);
    interp->framePtr = savedFramePtr;

    return objPtr;
}

int Jim_UnsetVariable(Jim_Interp *interp, Jim_Obj *nameObjPtr, int flags)
{
    Jim_Var *varPtr;
    int retval;
    Jim_CallFrame *framePtr;

    retval = SetVariableFromAny(interp, nameObjPtr);
    if (retval == JIM_DICT_SUGAR) {

        return JimDictSugarSet(interp, nameObjPtr, NULL);
    }
    else if (retval == JIM_OK) {
        varPtr = nameObjPtr->internalRep.varValue.varPtr;


        if (varPtr->linkFramePtr) {
            framePtr = interp->framePtr;
            interp->framePtr = varPtr->linkFramePtr;
            retval = Jim_UnsetVariable(interp, varPtr->objPtr, JIM_NONE);
            interp->framePtr = framePtr;
        }
        else {
            const char *name = Jim_String(nameObjPtr);
            if (nameObjPtr->internalRep.varValue.global) {
                name += 2;
                framePtr = interp->topFramePtr;
            }
            else {
                framePtr = interp->framePtr;
            }

            retval = Jim_DeleteHashEntry(&framePtr->vars, name);
            if (retval == JIM_OK) {

                framePtr->id = interp->callFrameEpoch++;
            }
        }
    }
    if (retval != JIM_OK && (flags & JIM_ERRMSG)) {
        Jim_SetResultFormatted(interp, "can't unset \"%#s\": no such variable", nameObjPtr);
    }
    return retval;
}



static void JimDictSugarParseVarKey(Jim_Interp *interp, Jim_Obj *objPtr,
    Jim_Obj **varPtrPtr, Jim_Obj **keyPtrPtr)
{
    const char *str, *p;
    int len, keyLen;
    Jim_Obj *varObjPtr, *keyObjPtr;

    str = Jim_GetString(objPtr, &len);

    p = strchr(str, '(');
    JimPanic((p == NULL, "JimDictSugarParseVarKey() called for non-dict-sugar (%s)", str));

    varObjPtr = Jim_NewStringObj(interp, str, p - str);

    p++;
    keyLen = (str + len) - p;
    if (str[len - 1] == ')') {
        keyLen--;
    }


    keyObjPtr = Jim_NewStringObj(interp, p, keyLen);

    Jim_IncrRefCount(varObjPtr);
    Jim_IncrRefCount(keyObjPtr);
    *varPtrPtr = varObjPtr;
    *keyPtrPtr = keyObjPtr;
}

static int JimDictSugarSet(Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *valObjPtr)
{
    int err;

    SetDictSubstFromAny(interp, objPtr);

    err = Jim_SetDictKeysVector(interp, objPtr->internalRep.dictSubstValue.varNameObjPtr,
        &objPtr->internalRep.dictSubstValue.indexObjPtr, 1, valObjPtr, JIM_MUSTEXIST);

    if (err == JIM_OK) {

        Jim_SetEmptyResult(interp);
    }
    else {
        if (!valObjPtr) {

            if (Jim_GetVariable(interp, objPtr->internalRep.dictSubstValue.varNameObjPtr, JIM_NONE)) {
                Jim_SetResultFormatted(interp, "can't unset \"%#s\": no such element in array",
                    objPtr);
                return err;
            }
        }

        Jim_SetResultFormatted(interp, "can't %s \"%#s\": variable isn't array",
            (valObjPtr ? "set" : "unset"), objPtr);
    }
    return err;
}

static Jim_Obj *JimDictExpandArrayVariable(Jim_Interp *interp, Jim_Obj *varObjPtr,
    Jim_Obj *keyObjPtr, int flags)
{
    Jim_Obj *dictObjPtr;
    Jim_Obj *resObjPtr = NULL;
    int ret;

    dictObjPtr = Jim_GetVariable(interp, varObjPtr, JIM_ERRMSG);
    if (!dictObjPtr) {
        return NULL;
    }

    ret = Jim_DictKey(interp, dictObjPtr, keyObjPtr, &resObjPtr, JIM_NONE);
    if (ret != JIM_OK) {
        Jim_SetResultFormatted(interp,
            "can't read \"%#s(%#s)\": %s array", varObjPtr, keyObjPtr,
            ret < 0 ? "variable isn't" : "no such element in");
    }
    else if ((flags & JIM_UNSHARED) && Jim_IsShared(dictObjPtr)) {

        Jim_SetVariable(interp, varObjPtr, Jim_DuplicateObj(interp, dictObjPtr));
    }

    return resObjPtr;
}


static Jim_Obj *JimDictSugarGet(Jim_Interp *interp, Jim_Obj *objPtr, int flags)
{
    SetDictSubstFromAny(interp, objPtr);

    return JimDictExpandArrayVariable(interp,
        objPtr->internalRep.dictSubstValue.varNameObjPtr,
        objPtr->internalRep.dictSubstValue.indexObjPtr, flags);
}



void FreeDictSubstInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_DecrRefCount(interp, objPtr->internalRep.dictSubstValue.varNameObjPtr);
    Jim_DecrRefCount(interp, objPtr->internalRep.dictSubstValue.indexObjPtr);
}

static void DupDictSubstInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{

    dupPtr->internalRep = srcPtr->internalRep;

    Jim_IncrRefCount(dupPtr->internalRep.dictSubstValue.varNameObjPtr);
    Jim_IncrRefCount(dupPtr->internalRep.dictSubstValue.indexObjPtr);
}


static void SetDictSubstFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    if (objPtr->typePtr != &dictSubstObjType) {
        Jim_Obj *varObjPtr, *keyObjPtr;

        if (objPtr->typePtr == &interpolatedObjType) {


            varObjPtr = objPtr->internalRep.dictSubstValue.varNameObjPtr;
            keyObjPtr = objPtr->internalRep.dictSubstValue.indexObjPtr;

            Jim_IncrRefCount(varObjPtr);
            Jim_IncrRefCount(keyObjPtr);
        }
        else {
            JimDictSugarParseVarKey(interp, objPtr, &varObjPtr, &keyObjPtr);
        }

        Jim_FreeIntRep(interp, objPtr);
        objPtr->typePtr = &dictSubstObjType;
        objPtr->internalRep.dictSubstValue.varNameObjPtr = varObjPtr;
        objPtr->internalRep.dictSubstValue.indexObjPtr = keyObjPtr;
    }
}

static Jim_Obj *JimExpandDictSugar(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_Obj *resObjPtr = NULL;
    Jim_Obj *substKeyObjPtr = NULL;

    SetDictSubstFromAny(interp, objPtr);

    if (Jim_SubstObj(interp, objPtr->internalRep.dictSubstValue.indexObjPtr,
            &substKeyObjPtr, JIM_NONE)
        != JIM_OK) {
        return NULL;
    }
    Jim_IncrRefCount(substKeyObjPtr);
    resObjPtr =
        JimDictExpandArrayVariable(interp, objPtr->internalRep.dictSubstValue.varNameObjPtr,
        substKeyObjPtr, 0);
    Jim_DecrRefCount(interp, substKeyObjPtr);

    return resObjPtr;
}

static Jim_Obj *JimExpandExprSugar(Jim_Interp *interp, Jim_Obj *objPtr)
{
    if (Jim_EvalExpression(interp, objPtr) == JIM_OK) {
        return Jim_GetResult(interp);
    }
    return NULL;
}


static Jim_CallFrame *JimCreateCallFrame(Jim_Interp *interp, Jim_CallFrame *parent, Jim_Obj *nsObj)
{
    Jim_CallFrame *cf;

    if (interp->freeFramesList) {
        cf = interp->freeFramesList;
        interp->freeFramesList = cf->next;

        cf->argv = NULL;
        cf->argc = 0;
        cf->procArgsObjPtr = NULL;
        cf->procBodyObjPtr = NULL;
        cf->next = NULL;
        cf->staticVars = NULL;
        cf->localCommands = NULL;
        cf->tailcallObj = NULL;
        cf->tailcallCmd = NULL;
    }
    else {
        cf = Jim_Alloc(sizeof(*cf));
        memset(cf, 0, sizeof(*cf));

        Jim_InitHashTable(&cf->vars, &JimVariablesHashTableType, interp);
    }

    cf->id = interp->callFrameEpoch++;
    cf->parent = parent;
    cf->level = parent ? parent->level + 1 : 0;
    cf->nsObj = nsObj;
    Jim_IncrRefCount(nsObj);

    return cf;
}

static int JimDeleteLocalProcs(Jim_Interp *interp, Jim_Stack *localCommands)
{

    if (localCommands) {
        Jim_Obj *cmdNameObj;

        while ((cmdNameObj = Jim_StackPop(localCommands)) != NULL) {
            Jim_HashEntry *he;
            Jim_Obj *fqObjName;
            Jim_HashTable *ht = &interp->commands;

            const char *fqname = JimQualifyName(interp, Jim_String(cmdNameObj), &fqObjName);

            he = Jim_FindHashEntry(ht, fqname);

            if (he) {
                Jim_Cmd *cmd = Jim_GetHashEntryVal(he);
                if (cmd->prevCmd) {
                    Jim_Cmd *prevCmd = cmd->prevCmd;
                    cmd->prevCmd = NULL;


                    JimDecrCmdRefCount(interp, cmd);


                    Jim_SetHashVal(ht, he, prevCmd);
                }
                else {
                    Jim_DeleteHashEntry(ht, fqname);
                }
                Jim_InterpIncrProcEpoch(interp);
            }
            Jim_DecrRefCount(interp, cmdNameObj);
            JimFreeQualifiedName(interp, fqObjName);
        }
        Jim_FreeStack(localCommands);
        Jim_Free(localCommands);
    }
    return JIM_OK;
}

static int JimInvokeDefer(Jim_Interp *interp, int retcode)
{
    Jim_Obj *objPtr;


    if (Jim_FindHashEntry(&interp->framePtr->vars, "jim::defer") == NULL) {
        return retcode;
    }

    objPtr = Jim_GetVariableStr(interp, "jim::defer", JIM_NONE);

    if (objPtr) {
        int ret = JIM_OK;
        int i;
        int listLen = Jim_ListLength(interp, objPtr);
        Jim_Obj *resultObjPtr;

        Jim_IncrRefCount(objPtr);

        resultObjPtr = Jim_GetResult(interp);
        Jim_IncrRefCount(resultObjPtr);
        Jim_SetEmptyResult(interp);


        for (i = listLen; i > 0; i--) {

            Jim_Obj *scriptObjPtr = Jim_ListGetIndex(interp, objPtr, i - 1);
            ret = Jim_EvalObj(interp, scriptObjPtr);
            if (ret != JIM_OK) {
                break;
            }
        }

        if (ret == JIM_OK || retcode == JIM_ERR) {

            Jim_SetResult(interp, resultObjPtr);
        }
        else {
            retcode = ret;
        }

        Jim_DecrRefCount(interp, resultObjPtr);
        Jim_DecrRefCount(interp, objPtr);
    }
    return retcode;
}

#define JIM_FCF_FULL 0
#define JIM_FCF_REUSE 1
static void JimFreeCallFrame(Jim_Interp *interp, Jim_CallFrame *cf, int action)
 {
    JimDeleteLocalProcs(interp, cf->localCommands);

    if (cf->procArgsObjPtr)
        Jim_DecrRefCount(interp, cf->procArgsObjPtr);
    if (cf->procBodyObjPtr)
        Jim_DecrRefCount(interp, cf->procBodyObjPtr);
    Jim_DecrRefCount(interp, cf->nsObj);
    if (action == JIM_FCF_FULL || cf->vars.size != JIM_HT_INITIAL_SIZE)
        Jim_FreeHashTable(&cf->vars);
    else {
        int i;
        Jim_HashEntry **table = cf->vars.table, *he;

        for (i = 0; i < JIM_HT_INITIAL_SIZE; i++) {
            he = table[i];
            while (he != NULL) {
                Jim_HashEntry *nextEntry = he->next;
                Jim_Var *varPtr = Jim_GetHashEntryVal(he);

                Jim_DecrRefCount(interp, varPtr->objPtr);
                Jim_Free(Jim_GetHashEntryKey(he));
                Jim_Free(varPtr);
                Jim_Free(he);
                table[i] = NULL;
                he = nextEntry;
            }
        }
        cf->vars.used = 0;
    }
    cf->next = interp->freeFramesList;
    interp->freeFramesList = cf;
}



int Jim_IsBigEndian(void)
{
    union {
        unsigned short s;
        unsigned char c[2];
    } uval = {0x0102};

    return uval.c[0] == 1;
}


Jim_Interp *Jim_CreateInterp(void)
{
    Jim_Interp *i = Jim_Alloc(sizeof(*i));

    memset(i, 0, sizeof(*i));

    i->maxCallFrameDepth = JIM_MAX_CALLFRAME_DEPTH;
    i->maxEvalDepth = JIM_MAX_EVAL_DEPTH;
    i->lastCollectTime = time(NULL);

    Jim_InitHashTable(&i->commands, &JimCommandsHashTableType, i);
#ifdef JIM_REFERENCES
    Jim_InitHashTable(&i->references, &JimReferencesHashTableType, i);
#endif
    Jim_InitHashTable(&i->assocData, &JimAssocDataHashTableType, i);
    Jim_InitHashTable(&i->packages, &JimPackageHashTableType, NULL);
    i->emptyObj = Jim_NewEmptyStringObj(i);
    i->trueObj = Jim_NewIntObj(i, 1);
    i->falseObj = Jim_NewIntObj(i, 0);
    i->framePtr = i->topFramePtr = JimCreateCallFrame(i, NULL, i->emptyObj);
    i->errorFileNameObj = i->emptyObj;
    i->result = i->emptyObj;
    i->stackTrace = Jim_NewListObj(i, NULL, 0);
    i->unknown = Jim_NewStringObj(i, "unknown", -1);
    i->errorProc = i->emptyObj;
    i->currentScriptObj = Jim_NewEmptyStringObj(i);
    i->nullScriptObj = Jim_NewEmptyStringObj(i);
    Jim_IncrRefCount(i->emptyObj);
    Jim_IncrRefCount(i->errorFileNameObj);
    Jim_IncrRefCount(i->result);
    Jim_IncrRefCount(i->stackTrace);
    Jim_IncrRefCount(i->unknown);
    Jim_IncrRefCount(i->currentScriptObj);
    Jim_IncrRefCount(i->nullScriptObj);
    Jim_IncrRefCount(i->errorProc);
    Jim_IncrRefCount(i->trueObj);
    Jim_IncrRefCount(i->falseObj);


    Jim_SetVariableStrWithStr(i, JIM_LIBPATH, TCL_LIBRARY);
    Jim_SetVariableStrWithStr(i, JIM_INTERACTIVE, "0");

    Jim_SetVariableStrWithStr(i, "tcl_platform(engine)", "Jim");
    Jim_SetVariableStrWithStr(i, "tcl_platform(os)", TCL_PLATFORM_OS);
    Jim_SetVariableStrWithStr(i, "tcl_platform(platform)", TCL_PLATFORM_PLATFORM);
    Jim_SetVariableStrWithStr(i, "tcl_platform(pathSeparator)", TCL_PLATFORM_PATH_SEPARATOR);
    Jim_SetVariableStrWithStr(i, "tcl_platform(byteOrder)", Jim_IsBigEndian() ? "bigEndian" : "littleEndian");
    Jim_SetVariableStrWithStr(i, "tcl_platform(threaded)", "0");
    Jim_SetVariableStr(i, "tcl_platform(pointerSize)", Jim_NewIntObj(i, sizeof(void *)));
    Jim_SetVariableStr(i, "tcl_platform(wordSize)", Jim_NewIntObj(i, sizeof(jim_wide)));

    return i;
}

void Jim_FreeInterp(Jim_Interp *i)
{
    Jim_CallFrame *cf, *cfx;

    Jim_Obj *objPtr, *nextObjPtr;


    for (cf = i->framePtr; cf; cf = cfx) {

        JimInvokeDefer(i, JIM_OK);
        cfx = cf->parent;
        JimFreeCallFrame(i, cf, JIM_FCF_FULL);
    }

    Jim_DecrRefCount(i, i->emptyObj);
    Jim_DecrRefCount(i, i->trueObj);
    Jim_DecrRefCount(i, i->falseObj);
    Jim_DecrRefCount(i, i->result);
    Jim_DecrRefCount(i, i->stackTrace);
    Jim_DecrRefCount(i, i->errorProc);
    Jim_DecrRefCount(i, i->unknown);
    Jim_DecrRefCount(i, i->errorFileNameObj);
    Jim_DecrRefCount(i, i->currentScriptObj);
    Jim_DecrRefCount(i, i->nullScriptObj);
    Jim_FreeHashTable(&i->commands);
#ifdef JIM_REFERENCES
    Jim_FreeHashTable(&i->references);
#endif
    Jim_FreeHashTable(&i->packages);
    Jim_Free(i->prngState);
    Jim_FreeHashTable(&i->assocData);

#ifdef JIM_MAINTAINER
    if (i->liveList != NULL) {
        objPtr = i->liveList;

        printf("\n-------------------------------------\n");
        printf("Objects still in the free list:\n");
        while (objPtr) {
            const char *type = objPtr->typePtr ? objPtr->typePtr->name : "string";
            Jim_String(objPtr);

            if (objPtr->bytes && strlen(objPtr->bytes) > 20) {
                printf("%p (%d) %-10s: '%.20s...'\n",
                    (void *)objPtr, objPtr->refCount, type, objPtr->bytes);
            }
            else {
                printf("%p (%d) %-10s: '%s'\n",
                    (void *)objPtr, objPtr->refCount, type, objPtr->bytes ? objPtr->bytes : "(null)");
            }
            if (objPtr->typePtr == &sourceObjType) {
                printf("FILE %s LINE %d\n",
                    Jim_String(objPtr->internalRep.sourceValue.fileNameObj),
                    objPtr->internalRep.sourceValue.lineNumber);
            }
            objPtr = objPtr->nextObjPtr;
        }
        printf("-------------------------------------\n\n");
        JimPanic((1, "Live list non empty freeing the interpreter! Leak?"));
    }
#endif


    objPtr = i->freeList;
    while (objPtr) {
        nextObjPtr = objPtr->nextObjPtr;
        Jim_Free(objPtr);
        objPtr = nextObjPtr;
    }


    for (cf = i->freeFramesList; cf; cf = cfx) {
        cfx = cf->next;
        if (cf->vars.table)
            Jim_FreeHashTable(&cf->vars);
        Jim_Free(cf);
    }


    Jim_Free(i);
}

Jim_CallFrame *Jim_GetCallFrameByLevel(Jim_Interp *interp, Jim_Obj *levelObjPtr)
{
    long level;
    const char *str;
    Jim_CallFrame *framePtr;

    if (levelObjPtr) {
        str = Jim_String(levelObjPtr);
        if (str[0] == '#') {
            char *endptr;

            level = jim_strtol(str + 1, &endptr);
            if (str[1] == '\0' || endptr[0] != '\0') {
                level = -1;
            }
        }
        else {
            if (Jim_GetLong(interp, levelObjPtr, &level) != JIM_OK || level < 0) {
                level = -1;
            }
            else {

                level = interp->framePtr->level - level;
            }
        }
    }
    else {
        str = "1";
        level = interp->framePtr->level - 1;
    }

    if (level == 0) {
        return interp->topFramePtr;
    }
    if (level > 0) {

        for (framePtr = interp->framePtr; framePtr; framePtr = framePtr->parent) {
            if (framePtr->level == level) {
                return framePtr;
            }
        }
    }

    Jim_SetResultFormatted(interp, "bad level \"%s\"", str);
    return NULL;
}

static Jim_CallFrame *JimGetCallFrameByInteger(Jim_Interp *interp, Jim_Obj *levelObjPtr)
{
    long level;
    Jim_CallFrame *framePtr;

    if (Jim_GetLong(interp, levelObjPtr, &level) == JIM_OK) {
        if (level <= 0) {

            level = interp->framePtr->level + level;
        }

        if (level == 0) {
            return interp->topFramePtr;
        }


        for (framePtr = interp->framePtr; framePtr; framePtr = framePtr->parent) {
            if (framePtr->level == level) {
                return framePtr;
            }
        }
    }

    Jim_SetResultFormatted(interp, "bad level \"%#s\"", levelObjPtr);
    return NULL;
}

static void JimResetStackTrace(Jim_Interp *interp)
{
    Jim_DecrRefCount(interp, interp->stackTrace);
    interp->stackTrace = Jim_NewListObj(interp, NULL, 0);
    Jim_IncrRefCount(interp->stackTrace);
}

static void JimSetStackTrace(Jim_Interp *interp, Jim_Obj *stackTraceObj)
{
    int len;


    Jim_IncrRefCount(stackTraceObj);
    Jim_DecrRefCount(interp, interp->stackTrace);
    interp->stackTrace = stackTraceObj;
    interp->errorFlag = 1;

    len = Jim_ListLength(interp, interp->stackTrace);
    if (len >= 3) {
        if (Jim_Length(Jim_ListGetIndex(interp, interp->stackTrace, len - 2)) == 0) {
            interp->addStackTrace = 1;
        }
    }
}

static void JimAppendStackTrace(Jim_Interp *interp, const char *procname,
    Jim_Obj *fileNameObj, int linenr)
{
    if (strcmp(procname, "unknown") == 0) {
        procname = "";
    }
    if (!*procname && !Jim_Length(fileNameObj)) {

        return;
    }

    if (Jim_IsShared(interp->stackTrace)) {
        Jim_DecrRefCount(interp, interp->stackTrace);
        interp->stackTrace = Jim_DuplicateObj(interp, interp->stackTrace);
        Jim_IncrRefCount(interp->stackTrace);
    }


    if (!*procname && Jim_Length(fileNameObj)) {

        int len = Jim_ListLength(interp, interp->stackTrace);

        if (len >= 3) {
            Jim_Obj *objPtr = Jim_ListGetIndex(interp, interp->stackTrace, len - 3);
            if (Jim_Length(objPtr)) {

                objPtr = Jim_ListGetIndex(interp, interp->stackTrace, len - 2);
                if (Jim_Length(objPtr) == 0) {

                    ListSetIndex(interp, interp->stackTrace, len - 2, fileNameObj, 0);
                    ListSetIndex(interp, interp->stackTrace, len - 1, Jim_NewIntObj(interp, linenr), 0);
                    return;
                }
            }
        }
    }

    Jim_ListAppendElement(interp, interp->stackTrace, Jim_NewStringObj(interp, procname, -1));
    Jim_ListAppendElement(interp, interp->stackTrace, fileNameObj);
    Jim_ListAppendElement(interp, interp->stackTrace, Jim_NewIntObj(interp, linenr));
}

int Jim_SetAssocData(Jim_Interp *interp, const char *key, Jim_InterpDeleteProc * delProc,
    void *data)
{
    AssocDataValue *assocEntryPtr = (AssocDataValue *) Jim_Alloc(sizeof(AssocDataValue));

    assocEntryPtr->delProc = delProc;
    assocEntryPtr->data = data;
    return Jim_AddHashEntry(&interp->assocData, key, assocEntryPtr);
}

void *Jim_GetAssocData(Jim_Interp *interp, const char *key)
{
    Jim_HashEntry *entryPtr = Jim_FindHashEntry(&interp->assocData, key);

    if (entryPtr != NULL) {
        AssocDataValue *assocEntryPtr = Jim_GetHashEntryVal(entryPtr);
        return assocEntryPtr->data;
    }
    return NULL;
}

int Jim_DeleteAssocData(Jim_Interp *interp, const char *key)
{
    return Jim_DeleteHashEntry(&interp->assocData, key);
}

int Jim_GetExitCode(Jim_Interp *interp)
{
    return interp->exitCode;
}

static void UpdateStringOfInt(struct Jim_Obj *objPtr);
static int SetIntFromAny(Jim_Interp *interp, Jim_Obj *objPtr, int flags);

static const Jim_ObjType intObjType = {
    "int",
    NULL,
    NULL,
    UpdateStringOfInt,
    JIM_TYPE_NONE,
};

static const Jim_ObjType coercedDoubleObjType = {
    "coerced-double",
    NULL,
    NULL,
    UpdateStringOfInt,
    JIM_TYPE_NONE,
};


static void UpdateStringOfInt(struct Jim_Obj *objPtr)
{
    char buf[JIM_INTEGER_SPACE + 1];
    jim_wide wideValue = JimWideValue(objPtr);
    int pos = 0;

    if (wideValue == 0) {
        buf[pos++] = '0';
    }
    else {
        char tmp[JIM_INTEGER_SPACE];
        int num = 0;
        int i;

        if (wideValue < 0) {
            buf[pos++] = '-';
            i = wideValue % 10;
            tmp[num++] = (i > 0) ? (10 - i) : -i;
            wideValue /= -10;
        }

        while (wideValue) {
            tmp[num++] = wideValue % 10;
            wideValue /= 10;
        }

        for (i = 0; i < num; i++) {
            buf[pos++] = '0' + tmp[num - i - 1];
        }
    }
    buf[pos] = 0;

    JimSetStringBytes(objPtr, buf);
}

static int SetIntFromAny(Jim_Interp *interp, Jim_Obj *objPtr, int flags)
{
    jim_wide wideValue;
    const char *str;

    if (objPtr->typePtr == &coercedDoubleObjType) {

        objPtr->typePtr = &intObjType;
        return JIM_OK;
    }


    str = Jim_String(objPtr);

    if (Jim_StringToWide(str, &wideValue, 0) != JIM_OK) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultFormatted(interp, "expected integer but got \"%#s\"", objPtr);
        }
        return JIM_ERR;
    }
    if ((wideValue == JIM_WIDE_MIN || wideValue == JIM_WIDE_MAX) && errno == ERANGE) {
        Jim_SetResultString(interp, "Integer value too big to be represented", -1);
        return JIM_ERR;
    }

    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &intObjType;
    objPtr->internalRep.wideValue = wideValue;
    return JIM_OK;
}

#ifdef JIM_OPTIMIZATION
static int JimIsWide(Jim_Obj *objPtr)
{
    return objPtr->typePtr == &intObjType;
}
#endif

int Jim_GetWide(Jim_Interp *interp, Jim_Obj *objPtr, jim_wide * widePtr)
{
    if (objPtr->typePtr != &intObjType && SetIntFromAny(interp, objPtr, JIM_ERRMSG) == JIM_ERR)
        return JIM_ERR;
    *widePtr = JimWideValue(objPtr);
    return JIM_OK;
}


static int JimGetWideNoErr(Jim_Interp *interp, Jim_Obj *objPtr, jim_wide * widePtr)
{
    if (objPtr->typePtr != &intObjType && SetIntFromAny(interp, objPtr, JIM_NONE) == JIM_ERR)
        return JIM_ERR;
    *widePtr = JimWideValue(objPtr);
    return JIM_OK;
}

int Jim_GetLong(Jim_Interp *interp, Jim_Obj *objPtr, long *longPtr)
{
    jim_wide wideValue;
    int retval;

    retval = Jim_GetWide(interp, objPtr, &wideValue);
    if (retval == JIM_OK) {
        *longPtr = (long)wideValue;
        return JIM_OK;
    }
    return JIM_ERR;
}

Jim_Obj *Jim_NewIntObj(Jim_Interp *interp, jim_wide wideValue)
{
    Jim_Obj *objPtr;

    objPtr = Jim_NewObj(interp);
    objPtr->typePtr = &intObjType;
    objPtr->bytes = NULL;
    objPtr->internalRep.wideValue = wideValue;
    return objPtr;
}

#define JIM_DOUBLE_SPACE 30

static void UpdateStringOfDouble(struct Jim_Obj *objPtr);
static int SetDoubleFromAny(Jim_Interp *interp, Jim_Obj *objPtr);

static const Jim_ObjType doubleObjType = {
    "double",
    NULL,
    NULL,
    UpdateStringOfDouble,
    JIM_TYPE_NONE,
};

#ifndef HAVE_ISNAN
#undef isnan
#define isnan(X) ((X) != (X))
#endif
#ifndef HAVE_ISINF
#undef isinf
#define isinf(X) (1.0 / (X) == 0.0)
#endif

static void UpdateStringOfDouble(struct Jim_Obj *objPtr)
{
    double value = objPtr->internalRep.doubleValue;

    if (isnan(value)) {
        JimSetStringBytes(objPtr, "NaN");
        return;
    }
    if (isinf(value)) {
        if (value < 0) {
            JimSetStringBytes(objPtr, "-Inf");
        }
        else {
            JimSetStringBytes(objPtr, "Inf");
        }
        return;
    }
    {
        char buf[JIM_DOUBLE_SPACE + 1];
        int i;
        int len = sprintf(buf, "%.12g", value);


        for (i = 0; i < len; i++) {
            if (buf[i] == '.' || buf[i] == 'e') {
#if defined(JIM_SPRINTF_DOUBLE_NEEDS_FIX)
                char *e = strchr(buf, 'e');
                if (e && (e[1] == '-' || e[1] == '+') && e[2] == '0') {

                    e += 2;
                    memmove(e, e + 1, len - (e - buf));
                }
#endif
                break;
            }
        }
        if (buf[i] == '\0') {
            buf[i++] = '.';
            buf[i++] = '0';
            buf[i] = '\0';
        }
        JimSetStringBytes(objPtr, buf);
    }
}

static int SetDoubleFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    double doubleValue;
    jim_wide wideValue;
    const char *str;

#ifdef HAVE_LONG_LONG

#define MIN_INT_IN_DOUBLE -(1LL << 53)
#define MAX_INT_IN_DOUBLE -(MIN_INT_IN_DOUBLE + 1)

    if (objPtr->typePtr == &intObjType
        && JimWideValue(objPtr) >= MIN_INT_IN_DOUBLE
        && JimWideValue(objPtr) <= MAX_INT_IN_DOUBLE) {


        objPtr->typePtr = &coercedDoubleObjType;
        return JIM_OK;
    }
#endif
    str = Jim_String(objPtr);

    if (Jim_StringToWide(str, &wideValue, 10) == JIM_OK) {

        Jim_FreeIntRep(interp, objPtr);
        objPtr->typePtr = &coercedDoubleObjType;
        objPtr->internalRep.wideValue = wideValue;
        return JIM_OK;
    }
    else {

        if (Jim_StringToDouble(str, &doubleValue) != JIM_OK) {
            Jim_SetResultFormatted(interp, "expected floating-point number but got \"%#s\"", objPtr);
            return JIM_ERR;
        }

        Jim_FreeIntRep(interp, objPtr);
    }
    objPtr->typePtr = &doubleObjType;
    objPtr->internalRep.doubleValue = doubleValue;
    return JIM_OK;
}

int Jim_GetDouble(Jim_Interp *interp, Jim_Obj *objPtr, double *doublePtr)
{
    if (objPtr->typePtr == &coercedDoubleObjType) {
        *doublePtr = JimWideValue(objPtr);
        return JIM_OK;
    }
    if (objPtr->typePtr != &doubleObjType && SetDoubleFromAny(interp, objPtr) == JIM_ERR)
        return JIM_ERR;

    if (objPtr->typePtr == &coercedDoubleObjType) {
        *doublePtr = JimWideValue(objPtr);
    }
    else {
        *doublePtr = objPtr->internalRep.doubleValue;
    }
    return JIM_OK;
}

Jim_Obj *Jim_NewDoubleObj(Jim_Interp *interp, double doubleValue)
{
    Jim_Obj *objPtr;

    objPtr = Jim_NewObj(interp);
    objPtr->typePtr = &doubleObjType;
    objPtr->bytes = NULL;
    objPtr->internalRep.doubleValue = doubleValue;
    return objPtr;
}

static int SetBooleanFromAny(Jim_Interp *interp, Jim_Obj *objPtr, int flags);

int Jim_GetBoolean(Jim_Interp *interp, Jim_Obj *objPtr, int * booleanPtr)
{
    if (objPtr->typePtr != &intObjType && SetBooleanFromAny(interp, objPtr, JIM_ERRMSG) == JIM_ERR)
        return JIM_ERR;
    *booleanPtr = (int) JimWideValue(objPtr);
    return JIM_OK;
}

static int SetBooleanFromAny(Jim_Interp *interp, Jim_Obj *objPtr, int flags)
{
    static const char * const falses[] = {
        "0", "false", "no", "off", NULL
    };
    static const char * const trues[] = {
        "1", "true", "yes", "on", NULL
    };

    int boolean;

    int index;
    if (Jim_GetEnum(interp, objPtr, falses, &index, NULL, 0) == JIM_OK) {
        boolean = 0;
    } else if (Jim_GetEnum(interp, objPtr, trues, &index, NULL, 0) == JIM_OK) {
        boolean = 1;
    } else {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultFormatted(interp, "expected boolean but got \"%#s\"", objPtr);
        }
        return JIM_ERR;
    }


    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &intObjType;
    objPtr->internalRep.wideValue = boolean;
    return JIM_OK;
}

static void ListInsertElements(Jim_Obj *listPtr, int idx, int elemc, Jim_Obj *const *elemVec);
static void ListAppendElement(Jim_Obj *listPtr, Jim_Obj *objPtr);
static void FreeListInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void DupListInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);
static void UpdateStringOfList(struct Jim_Obj *objPtr);
static int SetListFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr);

static const Jim_ObjType listObjType = {
    "list",
    FreeListInternalRep,
    DupListInternalRep,
    UpdateStringOfList,
    JIM_TYPE_NONE,
};

void FreeListInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    int i;

    for (i = 0; i < objPtr->internalRep.listValue.len; i++) {
        Jim_DecrRefCount(interp, objPtr->internalRep.listValue.ele[i]);
    }
    Jim_Free(objPtr->internalRep.listValue.ele);
}

void DupListInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    int i;

    JIM_NOTUSED(interp);

    dupPtr->internalRep.listValue.len = srcPtr->internalRep.listValue.len;
    dupPtr->internalRep.listValue.maxLen = srcPtr->internalRep.listValue.maxLen;
    dupPtr->internalRep.listValue.ele =
        Jim_Alloc(sizeof(Jim_Obj *) * srcPtr->internalRep.listValue.maxLen);
    memcpy(dupPtr->internalRep.listValue.ele, srcPtr->internalRep.listValue.ele,
        sizeof(Jim_Obj *) * srcPtr->internalRep.listValue.len);
    for (i = 0; i < dupPtr->internalRep.listValue.len; i++) {
        Jim_IncrRefCount(dupPtr->internalRep.listValue.ele[i]);
    }
    dupPtr->typePtr = &listObjType;
}

#define JIM_ELESTR_SIMPLE 0
#define JIM_ELESTR_BRACE 1
#define JIM_ELESTR_QUOTE 2
static unsigned char ListElementQuotingType(const char *s, int len)
{
    int i, level, blevel, trySimple = 1;


    if (len == 0)
        return JIM_ELESTR_BRACE;
    if (s[0] == '"' || s[0] == '{') {
        trySimple = 0;
        goto testbrace;
    }
    for (i = 0; i < len; i++) {
        switch (s[i]) {
            case ' ':
            case '$':
            case '"':
            case '[':
            case ']':
            case ';':
            case '\\':
            case '\r':
            case '\n':
            case '\t':
            case '\f':
            case '\v':
                trySimple = 0;

            case '{':
            case '}':
                goto testbrace;
        }
    }
    return JIM_ELESTR_SIMPLE;

  testbrace:

    if (s[len - 1] == '\\')
        return JIM_ELESTR_QUOTE;
    level = 0;
    blevel = 0;
    for (i = 0; i < len; i++) {
        switch (s[i]) {
            case '{':
                level++;
                break;
            case '}':
                level--;
                if (level < 0)
                    return JIM_ELESTR_QUOTE;
                break;
            case '[':
                blevel++;
                break;
            case ']':
                blevel--;
                break;
            case '\\':
                if (s[i + 1] == '\n')
                    return JIM_ELESTR_QUOTE;
                else if (s[i + 1] != '\0')
                    i++;
                break;
        }
    }
    if (blevel < 0) {
        return JIM_ELESTR_QUOTE;
    }

    if (level == 0) {
        if (!trySimple)
            return JIM_ELESTR_BRACE;
        for (i = 0; i < len; i++) {
            switch (s[i]) {
                case ' ':
                case '$':
                case '"':
                case '[':
                case ']':
                case ';':
                case '\\':
                case '\r':
                case '\n':
                case '\t':
                case '\f':
                case '\v':
                    return JIM_ELESTR_BRACE;
                    break;
            }
        }
        return JIM_ELESTR_SIMPLE;
    }
    return JIM_ELESTR_QUOTE;
}

static int BackslashQuoteString(const char *s, int len, char *q)
{
    char *p = q;

    while (len--) {
        switch (*s) {
            case ' ':
            case '$':
            case '"':
            case '[':
            case ']':
            case '{':
            case '}':
            case ';':
            case '\\':
                *p++ = '\\';
                *p++ = *s++;
                break;
            case '\n':
                *p++ = '\\';
                *p++ = 'n';
                s++;
                break;
            case '\r':
                *p++ = '\\';
                *p++ = 'r';
                s++;
                break;
            case '\t':
                *p++ = '\\';
                *p++ = 't';
                s++;
                break;
            case '\f':
                *p++ = '\\';
                *p++ = 'f';
                s++;
                break;
            case '\v':
                *p++ = '\\';
                *p++ = 'v';
                s++;
                break;
            default:
                *p++ = *s++;
                break;
        }
    }
    *p = '\0';

    return p - q;
}

static void JimMakeListStringRep(Jim_Obj *objPtr, Jim_Obj **objv, int objc)
{
    #define STATIC_QUOTING_LEN 32
    int i, bufLen, realLength;
    const char *strRep;
    char *p;
    unsigned char *quotingType, staticQuoting[STATIC_QUOTING_LEN];


    if (objc > STATIC_QUOTING_LEN) {
        quotingType = Jim_Alloc(objc);
    }
    else {
        quotingType = staticQuoting;
    }
    bufLen = 0;
    for (i = 0; i < objc; i++) {
        int len;

        strRep = Jim_GetString(objv[i], &len);
        quotingType[i] = ListElementQuotingType(strRep, len);
        switch (quotingType[i]) {
            case JIM_ELESTR_SIMPLE:
                if (i != 0 || strRep[0] != '#') {
                    bufLen += len;
                    break;
                }

                quotingType[i] = JIM_ELESTR_BRACE;

            case JIM_ELESTR_BRACE:
                bufLen += len + 2;
                break;
            case JIM_ELESTR_QUOTE:
                bufLen += len * 2;
                break;
        }
        bufLen++;
    }
    bufLen++;


    p = objPtr->bytes = Jim_Alloc(bufLen + 1);
    realLength = 0;
    for (i = 0; i < objc; i++) {
        int len, qlen;

        strRep = Jim_GetString(objv[i], &len);

        switch (quotingType[i]) {
            case JIM_ELESTR_SIMPLE:
                memcpy(p, strRep, len);
                p += len;
                realLength += len;
                break;
            case JIM_ELESTR_BRACE:
                *p++ = '{';
                memcpy(p, strRep, len);
                p += len;
                *p++ = '}';
                realLength += len + 2;
                break;
            case JIM_ELESTR_QUOTE:
                if (i == 0 && strRep[0] == '#') {
                    *p++ = '\\';
                    realLength++;
                }
                qlen = BackslashQuoteString(strRep, len, p);
                p += qlen;
                realLength += qlen;
                break;
        }

        if (i + 1 != objc) {
            *p++ = ' ';
            realLength++;
        }
    }
    *p = '\0';
    objPtr->length = realLength;

    if (quotingType != staticQuoting) {
        Jim_Free(quotingType);
    }
}

static void UpdateStringOfList(struct Jim_Obj *objPtr)
{
    JimMakeListStringRep(objPtr, objPtr->internalRep.listValue.ele, objPtr->internalRep.listValue.len);
}

static int SetListFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    struct JimParserCtx parser;
    const char *str;
    int strLen;
    Jim_Obj *fileNameObj;
    int linenr;

    if (objPtr->typePtr == &listObjType) {
        return JIM_OK;
    }

    if (Jim_IsDict(objPtr) && objPtr->bytes == NULL) {
        Jim_Obj **listObjPtrPtr;
        int len;
        int i;

        listObjPtrPtr = JimDictPairs(objPtr, &len);
        for (i = 0; i < len; i++) {
            Jim_IncrRefCount(listObjPtrPtr[i]);
        }


        Jim_FreeIntRep(interp, objPtr);
        objPtr->typePtr = &listObjType;
        objPtr->internalRep.listValue.len = len;
        objPtr->internalRep.listValue.maxLen = len;
        objPtr->internalRep.listValue.ele = listObjPtrPtr;

        return JIM_OK;
    }


    if (objPtr->typePtr == &sourceObjType) {
        fileNameObj = objPtr->internalRep.sourceValue.fileNameObj;
        linenr = objPtr->internalRep.sourceValue.lineNumber;
    }
    else {
        fileNameObj = interp->emptyObj;
        linenr = 1;
    }
    Jim_IncrRefCount(fileNameObj);


    str = Jim_GetString(objPtr, &strLen);

    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &listObjType;
    objPtr->internalRep.listValue.len = 0;
    objPtr->internalRep.listValue.maxLen = 0;
    objPtr->internalRep.listValue.ele = NULL;


    if (strLen) {
        JimParserInit(&parser, str, strLen, linenr);
        while (!parser.eof) {
            Jim_Obj *elementPtr;

            JimParseList(&parser);
            if (parser.tt != JIM_TT_STR && parser.tt != JIM_TT_ESC)
                continue;
            elementPtr = JimParserGetTokenObj(interp, &parser);
            JimSetSourceInfo(interp, elementPtr, fileNameObj, parser.tline);
            ListAppendElement(objPtr, elementPtr);
        }
    }
    Jim_DecrRefCount(interp, fileNameObj);
    return JIM_OK;
}

Jim_Obj *Jim_NewListObj(Jim_Interp *interp, Jim_Obj *const *elements, int len)
{
    Jim_Obj *objPtr;

    objPtr = Jim_NewObj(interp);
    objPtr->typePtr = &listObjType;
    objPtr->bytes = NULL;
    objPtr->internalRep.listValue.ele = NULL;
    objPtr->internalRep.listValue.len = 0;
    objPtr->internalRep.listValue.maxLen = 0;

    if (len) {
        ListInsertElements(objPtr, 0, len, elements);
    }

    return objPtr;
}

static void JimListGetElements(Jim_Interp *interp, Jim_Obj *listObj, int *listLen,
    Jim_Obj ***listVec)
{
    *listLen = Jim_ListLength(interp, listObj);
    *listVec = listObj->internalRep.listValue.ele;
}


static int JimSign(jim_wide w)
{
    if (w == 0) {
        return 0;
    }
    else if (w < 0) {
        return -1;
    }
    return 1;
}


struct lsort_info {
    jmp_buf jmpbuf;
    Jim_Obj *command;
    Jim_Interp *interp;
    enum {
        JIM_LSORT_ASCII,
        JIM_LSORT_NOCASE,
        JIM_LSORT_INTEGER,
        JIM_LSORT_REAL,
        JIM_LSORT_COMMAND
    } type;
    int order;
    int index;
    int indexed;
    int unique;
    int (*subfn)(Jim_Obj **, Jim_Obj **);
};

static struct lsort_info *sort_info;

static int ListSortIndexHelper(Jim_Obj **lhsObj, Jim_Obj **rhsObj)
{
    Jim_Obj *lObj, *rObj;

    if (Jim_ListIndex(sort_info->interp, *lhsObj, sort_info->index, &lObj, JIM_ERRMSG) != JIM_OK ||
        Jim_ListIndex(sort_info->interp, *rhsObj, sort_info->index, &rObj, JIM_ERRMSG) != JIM_OK) {
        longjmp(sort_info->jmpbuf, JIM_ERR);
    }
    return sort_info->subfn(&lObj, &rObj);
}


static int ListSortString(Jim_Obj **lhsObj, Jim_Obj **rhsObj)
{
    return Jim_StringCompareObj(sort_info->interp, *lhsObj, *rhsObj, 0) * sort_info->order;
}

static int ListSortStringNoCase(Jim_Obj **lhsObj, Jim_Obj **rhsObj)
{
    return Jim_StringCompareObj(sort_info->interp, *lhsObj, *rhsObj, 1) * sort_info->order;
}

static int ListSortInteger(Jim_Obj **lhsObj, Jim_Obj **rhsObj)
{
    jim_wide lhs = 0, rhs = 0;

    if (Jim_GetWide(sort_info->interp, *lhsObj, &lhs) != JIM_OK ||
        Jim_GetWide(sort_info->interp, *rhsObj, &rhs) != JIM_OK) {
        longjmp(sort_info->jmpbuf, JIM_ERR);
    }

    return JimSign(lhs - rhs) * sort_info->order;
}

static int ListSortReal(Jim_Obj **lhsObj, Jim_Obj **rhsObj)
{
    double lhs = 0, rhs = 0;

    if (Jim_GetDouble(sort_info->interp, *lhsObj, &lhs) != JIM_OK ||
        Jim_GetDouble(sort_info->interp, *rhsObj, &rhs) != JIM_OK) {
        longjmp(sort_info->jmpbuf, JIM_ERR);
    }
    if (lhs == rhs) {
        return 0;
    }
    if (lhs > rhs) {
        return sort_info->order;
    }
    return -sort_info->order;
}

static int ListSortCommand(Jim_Obj **lhsObj, Jim_Obj **rhsObj)
{
    Jim_Obj *compare_script;
    int rc;

    jim_wide ret = 0;


    compare_script = Jim_DuplicateObj(sort_info->interp, sort_info->command);
    Jim_ListAppendElement(sort_info->interp, compare_script, *lhsObj);
    Jim_ListAppendElement(sort_info->interp, compare_script, *rhsObj);

    rc = Jim_EvalObj(sort_info->interp, compare_script);

    if (rc != JIM_OK || Jim_GetWide(sort_info->interp, Jim_GetResult(sort_info->interp), &ret) != JIM_OK) {
        longjmp(sort_info->jmpbuf, rc);
    }

    return JimSign(ret) * sort_info->order;
}

static void ListRemoveDuplicates(Jim_Obj *listObjPtr, int (*comp)(Jim_Obj **lhs, Jim_Obj **rhs))
{
    int src;
    int dst = 0;
    Jim_Obj **ele = listObjPtr->internalRep.listValue.ele;

    for (src = 1; src < listObjPtr->internalRep.listValue.len; src++) {
        if (comp(&ele[dst], &ele[src]) == 0) {

            Jim_DecrRefCount(sort_info->interp, ele[dst]);
        }
        else {

            dst++;
        }
        ele[dst] = ele[src];
    }


    dst++;
    if (dst < listObjPtr->internalRep.listValue.len) {
        ele[dst] = ele[src];
    }


    listObjPtr->internalRep.listValue.len = dst;
}


static int ListSortElements(Jim_Interp *interp, Jim_Obj *listObjPtr, struct lsort_info *info)
{
    struct lsort_info *prev_info;

    typedef int (qsort_comparator) (const void *, const void *);
    int (*fn) (Jim_Obj **, Jim_Obj **);
    Jim_Obj **vector;
    int len;
    int rc;

    JimPanic((Jim_IsShared(listObjPtr), "ListSortElements called with shared object"));
    SetListFromAny(interp, listObjPtr);


    prev_info = sort_info;
    sort_info = info;

    vector = listObjPtr->internalRep.listValue.ele;
    len = listObjPtr->internalRep.listValue.len;
    switch (info->type) {
        case JIM_LSORT_ASCII:
            fn = ListSortString;
            break;
        case JIM_LSORT_NOCASE:
            fn = ListSortStringNoCase;
            break;
        case JIM_LSORT_INTEGER:
            fn = ListSortInteger;
            break;
        case JIM_LSORT_REAL:
            fn = ListSortReal;
            break;
        case JIM_LSORT_COMMAND:
            fn = ListSortCommand;
            break;
        default:
            fn = NULL;
            JimPanic((1, "ListSort called with invalid sort type"));
            return -1;
    }

    if (info->indexed) {

        info->subfn = fn;
        fn = ListSortIndexHelper;
    }

    if ((rc = setjmp(info->jmpbuf)) == 0) {
        qsort(vector, len, sizeof(Jim_Obj *), (qsort_comparator *) fn);

        if (info->unique && len > 1) {
            ListRemoveDuplicates(listObjPtr, fn);
        }

        Jim_InvalidateStringRep(listObjPtr);
    }
    sort_info = prev_info;

    return rc;
}

static void ListInsertElements(Jim_Obj *listPtr, int idx, int elemc, Jim_Obj *const *elemVec)
{
    int currentLen = listPtr->internalRep.listValue.len;
    int requiredLen = currentLen + elemc;
    int i;
    Jim_Obj **point;

    if (requiredLen > listPtr->internalRep.listValue.maxLen) {
        if (requiredLen < 2) {

            requiredLen = 4;
        }
        else {
            requiredLen *= 2;
        }

        listPtr->internalRep.listValue.ele = Jim_Realloc(listPtr->internalRep.listValue.ele,
            sizeof(Jim_Obj *) * requiredLen);

        listPtr->internalRep.listValue.maxLen = requiredLen;
    }
    if (idx < 0) {
        idx = currentLen;
    }
    point = listPtr->internalRep.listValue.ele + idx;
    memmove(point + elemc, point, (currentLen - idx) * sizeof(Jim_Obj *));
    for (i = 0; i < elemc; ++i) {
        point[i] = elemVec[i];
        Jim_IncrRefCount(point[i]);
    }
    listPtr->internalRep.listValue.len += elemc;
}

static void ListAppendElement(Jim_Obj *listPtr, Jim_Obj *objPtr)
{
    ListInsertElements(listPtr, -1, 1, &objPtr);
}

static void ListAppendList(Jim_Obj *listPtr, Jim_Obj *appendListPtr)
{
    ListInsertElements(listPtr, -1,
        appendListPtr->internalRep.listValue.len, appendListPtr->internalRep.listValue.ele);
}

void Jim_ListAppendElement(Jim_Interp *interp, Jim_Obj *listPtr, Jim_Obj *objPtr)
{
    JimPanic((Jim_IsShared(listPtr), "Jim_ListAppendElement called with shared object"));
    SetListFromAny(interp, listPtr);
    Jim_InvalidateStringRep(listPtr);
    ListAppendElement(listPtr, objPtr);
}

void Jim_ListAppendList(Jim_Interp *interp, Jim_Obj *listPtr, Jim_Obj *appendListPtr)
{
    JimPanic((Jim_IsShared(listPtr), "Jim_ListAppendList called with shared object"));
    SetListFromAny(interp, listPtr);
    SetListFromAny(interp, appendListPtr);
    Jim_InvalidateStringRep(listPtr);
    ListAppendList(listPtr, appendListPtr);
}

int Jim_ListLength(Jim_Interp *interp, Jim_Obj *objPtr)
{
    SetListFromAny(interp, objPtr);
    return objPtr->internalRep.listValue.len;
}

void Jim_ListInsertElements(Jim_Interp *interp, Jim_Obj *listPtr, int idx,
    int objc, Jim_Obj *const *objVec)
{
    JimPanic((Jim_IsShared(listPtr), "Jim_ListInsertElement called with shared object"));
    SetListFromAny(interp, listPtr);
    if (idx >= 0 && idx > listPtr->internalRep.listValue.len)
        idx = listPtr->internalRep.listValue.len;
    else if (idx < 0)
        idx = 0;
    Jim_InvalidateStringRep(listPtr);
    ListInsertElements(listPtr, idx, objc, objVec);
}

Jim_Obj *Jim_ListGetIndex(Jim_Interp *interp, Jim_Obj *listPtr, int idx)
{
    SetListFromAny(interp, listPtr);
    if ((idx >= 0 && idx >= listPtr->internalRep.listValue.len) ||
        (idx < 0 && (-idx - 1) >= listPtr->internalRep.listValue.len)) {
        return NULL;
    }
    if (idx < 0)
        idx = listPtr->internalRep.listValue.len + idx;
    return listPtr->internalRep.listValue.ele[idx];
}

int Jim_ListIndex(Jim_Interp *interp, Jim_Obj *listPtr, int idx, Jim_Obj **objPtrPtr, int flags)
{
    *objPtrPtr = Jim_ListGetIndex(interp, listPtr, idx);
    if (*objPtrPtr == NULL) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultString(interp, "list index out of range", -1);
        }
        return JIM_ERR;
    }
    return JIM_OK;
}

static int ListSetIndex(Jim_Interp *interp, Jim_Obj *listPtr, int idx,
    Jim_Obj *newObjPtr, int flags)
{
    SetListFromAny(interp, listPtr);
    if ((idx >= 0 && idx >= listPtr->internalRep.listValue.len) ||
        (idx < 0 && (-idx - 1) >= listPtr->internalRep.listValue.len)) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultString(interp, "list index out of range", -1);
        }
        return JIM_ERR;
    }
    if (idx < 0)
        idx = listPtr->internalRep.listValue.len + idx;
    Jim_DecrRefCount(interp, listPtr->internalRep.listValue.ele[idx]);
    listPtr->internalRep.listValue.ele[idx] = newObjPtr;
    Jim_IncrRefCount(newObjPtr);
    return JIM_OK;
}

int Jim_ListSetIndex(Jim_Interp *interp, Jim_Obj *varNamePtr,
    Jim_Obj *const *indexv, int indexc, Jim_Obj *newObjPtr)
{
    Jim_Obj *varObjPtr, *objPtr, *listObjPtr;
    int shared, i, idx;

    varObjPtr = objPtr = Jim_GetVariable(interp, varNamePtr, JIM_ERRMSG | JIM_UNSHARED);
    if (objPtr == NULL)
        return JIM_ERR;
    if ((shared = Jim_IsShared(objPtr)))
        varObjPtr = objPtr = Jim_DuplicateObj(interp, objPtr);
    for (i = 0; i < indexc - 1; i++) {
        listObjPtr = objPtr;
        if (Jim_GetIndex(interp, indexv[i], &idx) != JIM_OK)
            goto err;
        if (Jim_ListIndex(interp, listObjPtr, idx, &objPtr, JIM_ERRMSG) != JIM_OK) {
            goto err;
        }
        if (Jim_IsShared(objPtr)) {
            objPtr = Jim_DuplicateObj(interp, objPtr);
            ListSetIndex(interp, listObjPtr, idx, objPtr, JIM_NONE);
        }
        Jim_InvalidateStringRep(listObjPtr);
    }
    if (Jim_GetIndex(interp, indexv[indexc - 1], &idx) != JIM_OK)
        goto err;
    if (ListSetIndex(interp, objPtr, idx, newObjPtr, JIM_ERRMSG) == JIM_ERR)
        goto err;
    Jim_InvalidateStringRep(objPtr);
    Jim_InvalidateStringRep(varObjPtr);
    if (Jim_SetVariable(interp, varNamePtr, varObjPtr) != JIM_OK)
        goto err;
    Jim_SetResult(interp, varObjPtr);
    return JIM_OK;
  err:
    if (shared) {
        Jim_FreeNewObj(interp, varObjPtr);
    }
    return JIM_ERR;
}

Jim_Obj *Jim_ListJoin(Jim_Interp *interp, Jim_Obj *listObjPtr, const char *joinStr, int joinStrLen)
{
    int i;
    int listLen = Jim_ListLength(interp, listObjPtr);
    Jim_Obj *resObjPtr = Jim_NewEmptyStringObj(interp);

    for (i = 0; i < listLen; ) {
        Jim_AppendObj(interp, resObjPtr, Jim_ListGetIndex(interp, listObjPtr, i));
        if (++i != listLen) {
            Jim_AppendString(interp, resObjPtr, joinStr, joinStrLen);
        }
    }
    return resObjPtr;
}

Jim_Obj *Jim_ConcatObj(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    int i;

    for (i = 0; i < objc; i++) {
        if (!Jim_IsList(objv[i]))
            break;
    }
    if (i == objc) {
        Jim_Obj *objPtr = Jim_NewListObj(interp, NULL, 0);

        for (i = 0; i < objc; i++)
            ListAppendList(objPtr, objv[i]);
        return objPtr;
    }
    else {

        int len = 0, objLen;
        char *bytes, *p;


        for (i = 0; i < objc; i++) {
            len += Jim_Length(objv[i]);
        }
        if (objc)
            len += objc - 1;

        p = bytes = Jim_Alloc(len + 1);
        for (i = 0; i < objc; i++) {
            const char *s = Jim_GetString(objv[i], &objLen);


            while (objLen && isspace(UCHAR(*s))) {
                s++;
                objLen--;
                len--;
            }

            while (objLen && isspace(UCHAR(s[objLen - 1]))) {

                if (objLen > 1 && s[objLen - 2] == '\\') {
                    break;
                }
                objLen--;
                len--;
            }
            memcpy(p, s, objLen);
            p += objLen;
            if (i + 1 != objc) {
                if (objLen)
                    *p++ = ' ';
                else {
                    len--;
                }
            }
        }
        *p = '\0';
        return Jim_NewStringObjNoAlloc(interp, bytes, len);
    }
}

Jim_Obj *Jim_ListRange(Jim_Interp *interp, Jim_Obj *listObjPtr, Jim_Obj *firstObjPtr,
    Jim_Obj *lastObjPtr)
{
    int first, last;
    int len, rangeLen;

    if (Jim_GetIndex(interp, firstObjPtr, &first) != JIM_OK ||
        Jim_GetIndex(interp, lastObjPtr, &last) != JIM_OK)
        return NULL;
    len = Jim_ListLength(interp, listObjPtr);
    first = JimRelToAbsIndex(len, first);
    last = JimRelToAbsIndex(len, last);
    JimRelToAbsRange(len, &first, &last, &rangeLen);
    if (first == 0 && last == len) {
        return listObjPtr;
    }
    return Jim_NewListObj(interp, listObjPtr->internalRep.listValue.ele + first, rangeLen);
}

static void FreeDictInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void DupDictInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);
static void UpdateStringOfDict(struct Jim_Obj *objPtr);
static int SetDictFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr);


static unsigned int JimObjectHTHashFunction(const void *key)
{
    int len;
    const char *str = Jim_GetString((Jim_Obj *)key, &len);
    return Jim_GenHashFunction((const unsigned char *)str, len);
}

static int JimObjectHTKeyCompare(void *privdata, const void *key1, const void *key2)
{
    return Jim_StringEqObj((Jim_Obj *)key1, (Jim_Obj *)key2);
}

static void *JimObjectHTKeyValDup(void *privdata, const void *val)
{
    Jim_IncrRefCount((Jim_Obj *)val);
    return (void *)val;
}

static void JimObjectHTKeyValDestructor(void *interp, void *val)
{
    Jim_DecrRefCount(interp, (Jim_Obj *)val);
}

static const Jim_HashTableType JimDictHashTableType = {
    JimObjectHTHashFunction,
    JimObjectHTKeyValDup,
    JimObjectHTKeyValDup,
    JimObjectHTKeyCompare,
    JimObjectHTKeyValDestructor,
    JimObjectHTKeyValDestructor
};

static const Jim_ObjType dictObjType = {
    "dict",
    FreeDictInternalRep,
    DupDictInternalRep,
    UpdateStringOfDict,
    JIM_TYPE_NONE,
};

void FreeDictInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    JIM_NOTUSED(interp);

    Jim_FreeHashTable(objPtr->internalRep.ptr);
    Jim_Free(objPtr->internalRep.ptr);
}

void DupDictInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    Jim_HashTable *ht, *dupHt;
    Jim_HashTableIterator htiter;
    Jim_HashEntry *he;


    ht = srcPtr->internalRep.ptr;
    dupHt = Jim_Alloc(sizeof(*dupHt));
    Jim_InitHashTable(dupHt, &JimDictHashTableType, interp);
    if (ht->size != 0)
        Jim_ExpandHashTable(dupHt, ht->size);

    JimInitHashTableIterator(ht, &htiter);
    while ((he = Jim_NextHashEntry(&htiter)) != NULL) {
        Jim_AddHashEntry(dupHt, he->key, he->u.val);
    }

    dupPtr->internalRep.ptr = dupHt;
    dupPtr->typePtr = &dictObjType;
}

static Jim_Obj **JimDictPairs(Jim_Obj *dictPtr, int *len)
{
    Jim_HashTable *ht;
    Jim_HashTableIterator htiter;
    Jim_HashEntry *he;
    Jim_Obj **objv;
    int i;

    ht = dictPtr->internalRep.ptr;


    objv = Jim_Alloc((ht->used * 2) * sizeof(Jim_Obj *));
    JimInitHashTableIterator(ht, &htiter);
    i = 0;
    while ((he = Jim_NextHashEntry(&htiter)) != NULL) {
        objv[i++] = Jim_GetHashEntryKey(he);
        objv[i++] = Jim_GetHashEntryVal(he);
    }
    *len = i;
    return objv;
}

static void UpdateStringOfDict(struct Jim_Obj *objPtr)
{

    int len;
    Jim_Obj **objv = JimDictPairs(objPtr, &len);


    JimMakeListStringRep(objPtr, objv, len);

    Jim_Free(objv);
}

static int SetDictFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    int listlen;

    if (objPtr->typePtr == &dictObjType) {
        return JIM_OK;
    }

    if (Jim_IsList(objPtr) && Jim_IsShared(objPtr)) {
        Jim_String(objPtr);
    }


    listlen = Jim_ListLength(interp, objPtr);
    if (listlen % 2) {
        Jim_SetResultString(interp, "missing value to go with key", -1);
        return JIM_ERR;
    }
    else {

        Jim_HashTable *ht;
        int i;

        ht = Jim_Alloc(sizeof(*ht));
        Jim_InitHashTable(ht, &JimDictHashTableType, interp);

        for (i = 0; i < listlen; i += 2) {
            Jim_Obj *keyObjPtr = Jim_ListGetIndex(interp, objPtr, i);
            Jim_Obj *valObjPtr = Jim_ListGetIndex(interp, objPtr, i + 1);

            Jim_ReplaceHashEntry(ht, keyObjPtr, valObjPtr);
        }

        Jim_FreeIntRep(interp, objPtr);
        objPtr->typePtr = &dictObjType;
        objPtr->internalRep.ptr = ht;

        return JIM_OK;
    }
}



static int DictAddElement(Jim_Interp *interp, Jim_Obj *objPtr,
    Jim_Obj *keyObjPtr, Jim_Obj *valueObjPtr)
{
    Jim_HashTable *ht = objPtr->internalRep.ptr;

    if (valueObjPtr == NULL) {
        return Jim_DeleteHashEntry(ht, keyObjPtr);
    }
    Jim_ReplaceHashEntry(ht, keyObjPtr, valueObjPtr);
    return JIM_OK;
}

int Jim_DictAddElement(Jim_Interp *interp, Jim_Obj *objPtr,
    Jim_Obj *keyObjPtr, Jim_Obj *valueObjPtr)
{
    JimPanic((Jim_IsShared(objPtr), "Jim_DictAddElement called with shared object"));
    if (SetDictFromAny(interp, objPtr) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_InvalidateStringRep(objPtr);
    return DictAddElement(interp, objPtr, keyObjPtr, valueObjPtr);
}

Jim_Obj *Jim_NewDictObj(Jim_Interp *interp, Jim_Obj *const *elements, int len)
{
    Jim_Obj *objPtr;
    int i;

    JimPanic((len % 2, "Jim_NewDictObj() 'len' argument must be even"));

    objPtr = Jim_NewObj(interp);
    objPtr->typePtr = &dictObjType;
    objPtr->bytes = NULL;
    objPtr->internalRep.ptr = Jim_Alloc(sizeof(Jim_HashTable));
    Jim_InitHashTable(objPtr->internalRep.ptr, &JimDictHashTableType, interp);
    for (i = 0; i < len; i += 2)
        DictAddElement(interp, objPtr, elements[i], elements[i + 1]);
    return objPtr;
}

int Jim_DictKey(Jim_Interp *interp, Jim_Obj *dictPtr, Jim_Obj *keyPtr,
    Jim_Obj **objPtrPtr, int flags)
{
    Jim_HashEntry *he;
    Jim_HashTable *ht;

    if (SetDictFromAny(interp, dictPtr) != JIM_OK) {
        return -1;
    }
    ht = dictPtr->internalRep.ptr;
    if ((he = Jim_FindHashEntry(ht, keyPtr)) == NULL) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultFormatted(interp, "key \"%#s\" not known in dictionary", keyPtr);
        }
        return JIM_ERR;
    }
    else {
        *objPtrPtr = Jim_GetHashEntryVal(he);
        return JIM_OK;
    }
}


int Jim_DictPairs(Jim_Interp *interp, Jim_Obj *dictPtr, Jim_Obj ***objPtrPtr, int *len)
{
    if (SetDictFromAny(interp, dictPtr) != JIM_OK) {
        return JIM_ERR;
    }
    *objPtrPtr = JimDictPairs(dictPtr, len);

    return JIM_OK;
}



int Jim_DictKeysVector(Jim_Interp *interp, Jim_Obj *dictPtr,
    Jim_Obj *const *keyv, int keyc, Jim_Obj **objPtrPtr, int flags)
{
    int i;

    if (keyc == 0) {
        *objPtrPtr = dictPtr;
        return JIM_OK;
    }

    for (i = 0; i < keyc; i++) {
        Jim_Obj *objPtr;

        int rc = Jim_DictKey(interp, dictPtr, keyv[i], &objPtr, flags);
        if (rc != JIM_OK) {
            return rc;
        }
        dictPtr = objPtr;
    }
    *objPtrPtr = dictPtr;
    return JIM_OK;
}

int Jim_SetDictKeysVector(Jim_Interp *interp, Jim_Obj *varNamePtr,
    Jim_Obj *const *keyv, int keyc, Jim_Obj *newObjPtr, int flags)
{
    Jim_Obj *varObjPtr, *objPtr, *dictObjPtr;
    int shared, i;

    varObjPtr = objPtr = Jim_GetVariable(interp, varNamePtr, flags);
    if (objPtr == NULL) {
        if (newObjPtr == NULL && (flags & JIM_MUSTEXIST)) {

            return JIM_ERR;
        }
        varObjPtr = objPtr = Jim_NewDictObj(interp, NULL, 0);
        if (Jim_SetVariable(interp, varNamePtr, objPtr) != JIM_OK) {
            Jim_FreeNewObj(interp, varObjPtr);
            return JIM_ERR;
        }
    }
    if ((shared = Jim_IsShared(objPtr)))
        varObjPtr = objPtr = Jim_DuplicateObj(interp, objPtr);
    for (i = 0; i < keyc; i++) {
        dictObjPtr = objPtr;


        if (SetDictFromAny(interp, dictObjPtr) != JIM_OK) {
            goto err;
        }

        if (i == keyc - 1) {

            if (Jim_DictAddElement(interp, objPtr, keyv[keyc - 1], newObjPtr) != JIM_OK) {
                if (newObjPtr || (flags & JIM_MUSTEXIST)) {
                    goto err;
                }
            }
            break;
        }


        Jim_InvalidateStringRep(dictObjPtr);
        if (Jim_DictKey(interp, dictObjPtr, keyv[i], &objPtr,
                newObjPtr ? JIM_NONE : JIM_ERRMSG) == JIM_OK) {
            if (Jim_IsShared(objPtr)) {
                objPtr = Jim_DuplicateObj(interp, objPtr);
                DictAddElement(interp, dictObjPtr, keyv[i], objPtr);
            }
        }
        else {
            if (newObjPtr == NULL) {
                goto err;
            }
            objPtr = Jim_NewDictObj(interp, NULL, 0);
            DictAddElement(interp, dictObjPtr, keyv[i], objPtr);
        }
    }

    Jim_InvalidateStringRep(objPtr);
    Jim_InvalidateStringRep(varObjPtr);
    if (Jim_SetVariable(interp, varNamePtr, varObjPtr) != JIM_OK) {
        goto err;
    }
    Jim_SetResult(interp, varObjPtr);
    return JIM_OK;
  err:
    if (shared) {
        Jim_FreeNewObj(interp, varObjPtr);
    }
    return JIM_ERR;
}

static void UpdateStringOfIndex(struct Jim_Obj *objPtr);
static int SetIndexFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr);

static const Jim_ObjType indexObjType = {
    "index",
    NULL,
    NULL,
    UpdateStringOfIndex,
    JIM_TYPE_NONE,
};

static void UpdateStringOfIndex(struct Jim_Obj *objPtr)
{
    if (objPtr->internalRep.intValue == -1) {
        JimSetStringBytes(objPtr, "end");
    }
    else {
        char buf[JIM_INTEGER_SPACE + 1];
        if (objPtr->internalRep.intValue >= 0) {
            sprintf(buf, "%d", objPtr->internalRep.intValue);
        }
        else {

            sprintf(buf, "end%d", objPtr->internalRep.intValue + 1);
        }
        JimSetStringBytes(objPtr, buf);
    }
}

static int SetIndexFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    int idx, end = 0;
    const char *str;
    char *endptr;


    str = Jim_String(objPtr);


    if (strncmp(str, "end", 3) == 0) {
        end = 1;
        str += 3;
        idx = 0;
    }
    else {
        idx = jim_strtol(str, &endptr);

        if (endptr == str) {
            goto badindex;
        }
        str = endptr;
    }


    if (*str == '+' || *str == '-') {
        int sign = (*str == '+' ? 1 : -1);

        idx += sign * jim_strtol(++str, &endptr);
        if (str == endptr || *endptr) {
            goto badindex;
        }
        str = endptr;
    }

    while (isspace(UCHAR(*str))) {
        str++;
    }
    if (*str) {
        goto badindex;
    }
    if (end) {
        if (idx > 0) {
            idx = INT_MAX;
        }
        else {

            idx--;
        }
    }
    else if (idx < 0) {
        idx = -INT_MAX;
    }


    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &indexObjType;
    objPtr->internalRep.intValue = idx;
    return JIM_OK;

  badindex:
    Jim_SetResultFormatted(interp,
        "bad index \"%#s\": must be integer?[+-]integer? or end?[+-]integer?", objPtr);
    return JIM_ERR;
}

int Jim_GetIndex(Jim_Interp *interp, Jim_Obj *objPtr, int *indexPtr)
{

    if (objPtr->typePtr == &intObjType) {
        jim_wide val = JimWideValue(objPtr);

        if (val < 0)
            *indexPtr = -INT_MAX;
        else if (val > INT_MAX)
            *indexPtr = INT_MAX;
        else
            *indexPtr = (int)val;
        return JIM_OK;
    }
    if (objPtr->typePtr != &indexObjType && SetIndexFromAny(interp, objPtr) == JIM_ERR)
        return JIM_ERR;
    *indexPtr = objPtr->internalRep.intValue;
    return JIM_OK;
}



static const char * const jimReturnCodes[] = {
    "ok",
    "error",
    "return",
    "break",
    "continue",
    "signal",
    "exit",
    "eval",
    NULL
};

#define jimReturnCodesSize (sizeof(jimReturnCodes)/sizeof(*jimReturnCodes) - 1)

static const Jim_ObjType returnCodeObjType = {
    "return-code",
    NULL,
    NULL,
    NULL,
    JIM_TYPE_NONE,
};

const char *Jim_ReturnCode(int code)
{
    if (code < 0 || code >= (int)jimReturnCodesSize) {
        return "?";
    }
    else {
        return jimReturnCodes[code];
    }
}

static int SetReturnCodeFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    int returnCode;
    jim_wide wideValue;


    if (JimGetWideNoErr(interp, objPtr, &wideValue) != JIM_ERR)
        returnCode = (int)wideValue;
    else if (Jim_GetEnum(interp, objPtr, jimReturnCodes, &returnCode, NULL, JIM_NONE) != JIM_OK) {
        Jim_SetResultFormatted(interp, "expected return code but got \"%#s\"", objPtr);
        return JIM_ERR;
    }

    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &returnCodeObjType;
    objPtr->internalRep.intValue = returnCode;
    return JIM_OK;
}

int Jim_GetReturnCode(Jim_Interp *interp, Jim_Obj *objPtr, int *intPtr)
{
    if (objPtr->typePtr != &returnCodeObjType && SetReturnCodeFromAny(interp, objPtr) == JIM_ERR)
        return JIM_ERR;
    *intPtr = objPtr->internalRep.intValue;
    return JIM_OK;
}

static int JimParseExprOperator(struct JimParserCtx *pc);
static int JimParseExprNumber(struct JimParserCtx *pc);
static int JimParseExprIrrational(struct JimParserCtx *pc);
static int JimParseExprBoolean(struct JimParserCtx *pc);


enum
{



    JIM_EXPROP_MUL = JIM_TT_EXPR_OP,
    JIM_EXPROP_DIV,
    JIM_EXPROP_MOD,
    JIM_EXPROP_SUB,
    JIM_EXPROP_ADD,
    JIM_EXPROP_LSHIFT,
    JIM_EXPROP_RSHIFT,
    JIM_EXPROP_ROTL,
    JIM_EXPROP_ROTR,
    JIM_EXPROP_LT,
    JIM_EXPROP_GT,
    JIM_EXPROP_LTE,
    JIM_EXPROP_GTE,
    JIM_EXPROP_NUMEQ,
    JIM_EXPROP_NUMNE,
    JIM_EXPROP_BITAND,
    JIM_EXPROP_BITXOR,
    JIM_EXPROP_BITOR,
    JIM_EXPROP_LOGICAND,
    JIM_EXPROP_LOGICOR,
    JIM_EXPROP_TERNARY,
    JIM_EXPROP_COLON,
    JIM_EXPROP_POW,


    JIM_EXPROP_STREQ,
    JIM_EXPROP_STRNE,
    JIM_EXPROP_STRIN,
    JIM_EXPROP_STRNI,


    JIM_EXPROP_NOT,
    JIM_EXPROP_BITNOT,
    JIM_EXPROP_UNARYMINUS,
    JIM_EXPROP_UNARYPLUS,


    JIM_EXPROP_FUNC_INT,
    JIM_EXPROP_FUNC_WIDE,
    JIM_EXPROP_FUNC_ABS,
    JIM_EXPROP_FUNC_DOUBLE,
    JIM_EXPROP_FUNC_ROUND,
    JIM_EXPROP_FUNC_RAND,
    JIM_EXPROP_FUNC_SRAND,


    JIM_EXPROP_FUNC_SIN,
    JIM_EXPROP_FUNC_COS,
    JIM_EXPROP_FUNC_TAN,
    JIM_EXPROP_FUNC_ASIN,
    JIM_EXPROP_FUNC_ACOS,
    JIM_EXPROP_FUNC_ATAN,
    JIM_EXPROP_FUNC_ATAN2,
    JIM_EXPROP_FUNC_SINH,
    JIM_EXPROP_FUNC_COSH,
    JIM_EXPROP_FUNC_TANH,
    JIM_EXPROP_FUNC_CEIL,
    JIM_EXPROP_FUNC_FLOOR,
    JIM_EXPROP_FUNC_EXP,
    JIM_EXPROP_FUNC_LOG,
    JIM_EXPROP_FUNC_LOG10,
    JIM_EXPROP_FUNC_SQRT,
    JIM_EXPROP_FUNC_POW,
    JIM_EXPROP_FUNC_HYPOT,
    JIM_EXPROP_FUNC_FMOD,
};

struct JimExprNode {
    int type;
    struct Jim_Obj *objPtr;

    struct JimExprNode *left;
    struct JimExprNode *right;
    struct JimExprNode *ternary;
};


typedef struct Jim_ExprOperator
{
    const char *name;
    int (*funcop) (Jim_Interp *interp, struct JimExprNode *opnode);
    unsigned char precedence;
    unsigned char arity;
    unsigned char attr;
    unsigned char namelen;
} Jim_ExprOperator;

static int JimExprGetTerm(Jim_Interp *interp, struct JimExprNode *node, Jim_Obj **objPtrPtr);
static int JimExprGetTermBoolean(Jim_Interp *interp, struct JimExprNode *node);
static int JimExprEvalTermNode(Jim_Interp *interp, struct JimExprNode *node);

static int JimExprOpNumUnary(Jim_Interp *interp, struct JimExprNode *node)
{
    int intresult = 1;
    int rc;
    double dA, dC = 0;
    jim_wide wA, wC = 0;
    Jim_Obj *A;

    if ((rc = JimExprGetTerm(interp, node->left, &A)) != JIM_OK) {
        return rc;
    }

    if ((A->typePtr != &doubleObjType || A->bytes) && JimGetWideNoErr(interp, A, &wA) == JIM_OK) {
        switch (node->type) {
            case JIM_EXPROP_FUNC_INT:
            case JIM_EXPROP_FUNC_WIDE:
            case JIM_EXPROP_FUNC_ROUND:
            case JIM_EXPROP_UNARYPLUS:
                wC = wA;
                break;
            case JIM_EXPROP_FUNC_DOUBLE:
                dC = wA;
                intresult = 0;
                break;
            case JIM_EXPROP_FUNC_ABS:
                wC = wA >= 0 ? wA : -wA;
                break;
            case JIM_EXPROP_UNARYMINUS:
                wC = -wA;
                break;
            case JIM_EXPROP_NOT:
                wC = !wA;
                break;
            default:
                abort();
        }
    }
    else if ((rc = Jim_GetDouble(interp, A, &dA)) == JIM_OK) {
        switch (node->type) {
            case JIM_EXPROP_FUNC_INT:
            case JIM_EXPROP_FUNC_WIDE:
                wC = dA;
                break;
            case JIM_EXPROP_FUNC_ROUND:
                wC = dA < 0 ? (dA - 0.5) : (dA + 0.5);
                break;
            case JIM_EXPROP_FUNC_DOUBLE:
            case JIM_EXPROP_UNARYPLUS:
                dC = dA;
                intresult = 0;
                break;
            case JIM_EXPROP_FUNC_ABS:
#ifdef JIM_MATH_FUNCTIONS
                dC = fabs(dA);
#else
                dC = dA >= 0 ? dA : -dA;
#endif
                intresult = 0;
                break;
            case JIM_EXPROP_UNARYMINUS:
                dC = -dA;
                intresult = 0;
                break;
            case JIM_EXPROP_NOT:
                wC = !dA;
                break;
            default:
                abort();
        }
    }

    if (rc == JIM_OK) {
        if (intresult) {
            Jim_SetResultInt(interp, wC);
        }
        else {
            Jim_SetResult(interp, Jim_NewDoubleObj(interp, dC));
        }
    }

    Jim_DecrRefCount(interp, A);

    return rc;
}

static double JimRandDouble(Jim_Interp *interp)
{
    unsigned long x;
    JimRandomBytes(interp, &x, sizeof(x));

    return (double)x / (unsigned long)~0;
}

static int JimExprOpIntUnary(Jim_Interp *interp, struct JimExprNode *node)
{
    jim_wide wA;
    Jim_Obj *A;
    int rc;

    if ((rc = JimExprGetTerm(interp, node->left, &A)) != JIM_OK) {
        return rc;
    }

    rc = Jim_GetWide(interp, A, &wA);
    if (rc == JIM_OK) {
        switch (node->type) {
            case JIM_EXPROP_BITNOT:
                Jim_SetResultInt(interp, ~wA);
                break;
            case JIM_EXPROP_FUNC_SRAND:
                JimPrngSeed(interp, (unsigned char *)&wA, sizeof(wA));
                Jim_SetResult(interp, Jim_NewDoubleObj(interp, JimRandDouble(interp)));
                break;
            default:
                abort();
        }
    }

    Jim_DecrRefCount(interp, A);

    return rc;
}

static int JimExprOpNone(Jim_Interp *interp, struct JimExprNode *node)
{
    JimPanic((node->type != JIM_EXPROP_FUNC_RAND, "JimExprOpNone only support rand()"));

    Jim_SetResult(interp, Jim_NewDoubleObj(interp, JimRandDouble(interp)));

    return JIM_OK;
}

#ifdef JIM_MATH_FUNCTIONS
static int JimExprOpDoubleUnary(Jim_Interp *interp, struct JimExprNode *node)
{
    int rc;
    double dA, dC;
    Jim_Obj *A;

    if ((rc = JimExprGetTerm(interp, node->left, &A)) != JIM_OK) {
        return rc;
    }

    rc = Jim_GetDouble(interp, A, &dA);
    if (rc == JIM_OK) {
        switch (node->type) {
            case JIM_EXPROP_FUNC_SIN:
                dC = sin(dA);
                break;
            case JIM_EXPROP_FUNC_COS:
                dC = cos(dA);
                break;
            case JIM_EXPROP_FUNC_TAN:
                dC = tan(dA);
                break;
            case JIM_EXPROP_FUNC_ASIN:
                dC = asin(dA);
                break;
            case JIM_EXPROP_FUNC_ACOS:
                dC = acos(dA);
                break;
            case JIM_EXPROP_FUNC_ATAN:
                dC = atan(dA);
                break;
            case JIM_EXPROP_FUNC_SINH:
                dC = sinh(dA);
                break;
            case JIM_EXPROP_FUNC_COSH:
                dC = cosh(dA);
                break;
            case JIM_EXPROP_FUNC_TANH:
                dC = tanh(dA);
                break;
            case JIM_EXPROP_FUNC_CEIL:
                dC = ceil(dA);
                break;
            case JIM_EXPROP_FUNC_FLOOR:
                dC = floor(dA);
                break;
            case JIM_EXPROP_FUNC_EXP:
                dC = exp(dA);
                break;
            case JIM_EXPROP_FUNC_LOG:
                dC = log(dA);
                break;
            case JIM_EXPROP_FUNC_LOG10:
                dC = log10(dA);
                break;
            case JIM_EXPROP_FUNC_SQRT:
                dC = sqrt(dA);
                break;
            default:
                abort();
        }
        Jim_SetResult(interp, Jim_NewDoubleObj(interp, dC));
    }

    Jim_DecrRefCount(interp, A);

    return rc;
}
#endif


static int JimExprOpIntBin(Jim_Interp *interp, struct JimExprNode *node)
{
    jim_wide wA, wB;
    int rc;
    Jim_Obj *A, *B;

    if ((rc = JimExprGetTerm(interp, node->left, &A)) != JIM_OK) {
        return rc;
    }
    if ((rc = JimExprGetTerm(interp, node->right, &B)) != JIM_OK) {
        Jim_DecrRefCount(interp, A);
        return rc;
    }

    rc = JIM_ERR;

    if (Jim_GetWide(interp, A, &wA) == JIM_OK && Jim_GetWide(interp, B, &wB) == JIM_OK) {
        jim_wide wC;

        rc = JIM_OK;

        switch (node->type) {
            case JIM_EXPROP_LSHIFT:
                wC = wA << wB;
                break;
            case JIM_EXPROP_RSHIFT:
                wC = wA >> wB;
                break;
            case JIM_EXPROP_BITAND:
                wC = wA & wB;
                break;
            case JIM_EXPROP_BITXOR:
                wC = wA ^ wB;
                break;
            case JIM_EXPROP_BITOR:
                wC = wA | wB;
                break;
            case JIM_EXPROP_MOD:
                if (wB == 0) {
                    wC = 0;
                    Jim_SetResultString(interp, "Division by zero", -1);
                    rc = JIM_ERR;
                }
                else {
                    int negative = 0;

                    if (wB < 0) {
                        wB = -wB;
                        wA = -wA;
                        negative = 1;
                    }
                    wC = wA % wB;
                    if (wC < 0) {
                        wC += wB;
                    }
                    if (negative) {
                        wC = -wC;
                    }
                }
                break;
            case JIM_EXPROP_ROTL:
            case JIM_EXPROP_ROTR:{

                    unsigned long uA = (unsigned long)wA;
                    unsigned long uB = (unsigned long)wB;
                    const unsigned int S = sizeof(unsigned long) * 8;


                    uB %= S;

                    if (node->type == JIM_EXPROP_ROTR) {
                        uB = S - uB;
                    }
                    wC = (unsigned long)(uA << uB) | (uA >> (S - uB));
                    break;
                }
            default:
                abort();
        }
        Jim_SetResultInt(interp, wC);
    }

    Jim_DecrRefCount(interp, A);
    Jim_DecrRefCount(interp, B);

    return rc;
}



static int JimExprOpBin(Jim_Interp *interp, struct JimExprNode *node)
{
    int rc = JIM_OK;
    double dA, dB, dC = 0;
    jim_wide wA, wB, wC = 0;
    Jim_Obj *A, *B;

    if ((rc = JimExprGetTerm(interp, node->left, &A)) != JIM_OK) {
        return rc;
    }
    if ((rc = JimExprGetTerm(interp, node->right, &B)) != JIM_OK) {
        Jim_DecrRefCount(interp, A);
        return rc;
    }

    if ((A->typePtr != &doubleObjType || A->bytes) &&
        (B->typePtr != &doubleObjType || B->bytes) &&
        JimGetWideNoErr(interp, A, &wA) == JIM_OK && JimGetWideNoErr(interp, B, &wB) == JIM_OK) {



        switch (node->type) {
            case JIM_EXPROP_POW:
            case JIM_EXPROP_FUNC_POW:
                if (wA == 0 && wB < 0) {
                    Jim_SetResultString(interp, "exponentiation of zero by negative power", -1);
                    rc = JIM_ERR;
                    goto done;
                }
                wC = JimPowWide(wA, wB);
                goto intresult;
            case JIM_EXPROP_ADD:
                wC = wA + wB;
                goto intresult;
            case JIM_EXPROP_SUB:
                wC = wA - wB;
                goto intresult;
            case JIM_EXPROP_MUL:
                wC = wA * wB;
                goto intresult;
            case JIM_EXPROP_DIV:
                if (wB == 0) {
                    Jim_SetResultString(interp, "Division by zero", -1);
                    rc = JIM_ERR;
                    goto done;
                }
                else {
                    if (wB < 0) {
                        wB = -wB;
                        wA = -wA;
                    }
                    wC = wA / wB;
                    if (wA % wB < 0) {
                        wC--;
                    }
                    goto intresult;
                }
            case JIM_EXPROP_LT:
                wC = wA < wB;
                goto intresult;
            case JIM_EXPROP_GT:
                wC = wA > wB;
                goto intresult;
            case JIM_EXPROP_LTE:
                wC = wA <= wB;
                goto intresult;
            case JIM_EXPROP_GTE:
                wC = wA >= wB;
                goto intresult;
            case JIM_EXPROP_NUMEQ:
                wC = wA == wB;
                goto intresult;
            case JIM_EXPROP_NUMNE:
                wC = wA != wB;
                goto intresult;
        }
    }
    if (Jim_GetDouble(interp, A, &dA) == JIM_OK && Jim_GetDouble(interp, B, &dB) == JIM_OK) {
        switch (node->type) {
#ifndef JIM_MATH_FUNCTIONS
            case JIM_EXPROP_POW:
            case JIM_EXPROP_FUNC_POW:
            case JIM_EXPROP_FUNC_ATAN2:
            case JIM_EXPROP_FUNC_HYPOT:
            case JIM_EXPROP_FUNC_FMOD:
                Jim_SetResultString(interp, "unsupported", -1);
                rc = JIM_ERR;
                goto done;
#else
            case JIM_EXPROP_POW:
            case JIM_EXPROP_FUNC_POW:
                dC = pow(dA, dB);
                goto doubleresult;
            case JIM_EXPROP_FUNC_ATAN2:
                dC = atan2(dA, dB);
                goto doubleresult;
            case JIM_EXPROP_FUNC_HYPOT:
                dC = hypot(dA, dB);
                goto doubleresult;
            case JIM_EXPROP_FUNC_FMOD:
                dC = fmod(dA, dB);
                goto doubleresult;
#endif
            case JIM_EXPROP_ADD:
                dC = dA + dB;
                goto doubleresult;
            case JIM_EXPROP_SUB:
                dC = dA - dB;
                goto doubleresult;
            case JIM_EXPROP_MUL:
                dC = dA * dB;
                goto doubleresult;
            case JIM_EXPROP_DIV:
                if (dB == 0) {
#ifdef INFINITY
                    dC = dA < 0 ? -INFINITY : INFINITY;
#else
                    dC = (dA < 0 ? -1.0 : 1.0) * strtod("Inf", NULL);
#endif
                }
                else {
                    dC = dA / dB;
                }
                goto doubleresult;
            case JIM_EXPROP_LT:
                wC = dA < dB;
                goto intresult;
            case JIM_EXPROP_GT:
                wC = dA > dB;
                goto intresult;
            case JIM_EXPROP_LTE:
                wC = dA <= dB;
                goto intresult;
            case JIM_EXPROP_GTE:
                wC = dA >= dB;
                goto intresult;
            case JIM_EXPROP_NUMEQ:
                wC = dA == dB;
                goto intresult;
            case JIM_EXPROP_NUMNE:
                wC = dA != dB;
                goto intresult;
        }
    }
    else {



        int i = Jim_StringCompareObj(interp, A, B, 0);

        switch (node->type) {
            case JIM_EXPROP_LT:
                wC = i < 0;
                goto intresult;
            case JIM_EXPROP_GT:
                wC = i > 0;
                goto intresult;
            case JIM_EXPROP_LTE:
                wC = i <= 0;
                goto intresult;
            case JIM_EXPROP_GTE:
                wC = i >= 0;
                goto intresult;
            case JIM_EXPROP_NUMEQ:
                wC = i == 0;
                goto intresult;
            case JIM_EXPROP_NUMNE:
                wC = i != 0;
                goto intresult;
        }
    }

    rc = JIM_ERR;
done:
    Jim_DecrRefCount(interp, A);
    Jim_DecrRefCount(interp, B);
    return rc;
intresult:
    Jim_SetResultInt(interp, wC);
    goto done;
doubleresult:
    Jim_SetResult(interp, Jim_NewDoubleObj(interp, dC));
    goto done;
}

static int JimSearchList(Jim_Interp *interp, Jim_Obj *listObjPtr, Jim_Obj *valObj)
{
    int listlen;
    int i;

    listlen = Jim_ListLength(interp, listObjPtr);
    for (i = 0; i < listlen; i++) {
        if (Jim_StringEqObj(Jim_ListGetIndex(interp, listObjPtr, i), valObj)) {
            return 1;
        }
    }
    return 0;
}



static int JimExprOpStrBin(Jim_Interp *interp, struct JimExprNode *node)
{
    Jim_Obj *A, *B;
    jim_wide wC;
    int rc;

    if ((rc = JimExprGetTerm(interp, node->left, &A)) != JIM_OK) {
        return rc;
    }
    if ((rc = JimExprGetTerm(interp, node->right, &B)) != JIM_OK) {
        Jim_DecrRefCount(interp, A);
        return rc;
    }

    switch (node->type) {
        case JIM_EXPROP_STREQ:
        case JIM_EXPROP_STRNE:
            wC = Jim_StringEqObj(A, B);
            if (node->type == JIM_EXPROP_STRNE) {
                wC = !wC;
            }
            break;
        case JIM_EXPROP_STRIN:
            wC = JimSearchList(interp, B, A);
            break;
        case JIM_EXPROP_STRNI:
            wC = !JimSearchList(interp, B, A);
            break;
        default:
            abort();
    }
    Jim_SetResultInt(interp, wC);

    Jim_DecrRefCount(interp, A);
    Jim_DecrRefCount(interp, B);

    return rc;
}

static int ExprBool(Jim_Interp *interp, Jim_Obj *obj)
{
    long l;
    double d;
    int b;
    int ret = -1;


    Jim_IncrRefCount(obj);

    if (Jim_GetLong(interp, obj, &l) == JIM_OK) {
        ret = (l != 0);
    }
    else if (Jim_GetDouble(interp, obj, &d) == JIM_OK) {
        ret = (d != 0);
    }
    else if (Jim_GetBoolean(interp, obj, &b) == JIM_OK) {
        ret = (b != 0);
    }

    Jim_DecrRefCount(interp, obj);
    return ret;
}

static int JimExprOpAnd(Jim_Interp *interp, struct JimExprNode *node)
{

    int result = JimExprGetTermBoolean(interp, node->left);

    if (result == 1) {

        result = JimExprGetTermBoolean(interp, node->right);
    }
    if (result == -1) {
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, result);
    return JIM_OK;
}

static int JimExprOpOr(Jim_Interp *interp, struct JimExprNode *node)
{

    int result = JimExprGetTermBoolean(interp, node->left);

    if (result == 0) {

        result = JimExprGetTermBoolean(interp, node->right);
    }
    if (result == -1) {
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, result);
    return JIM_OK;
}

static int JimExprOpTernary(Jim_Interp *interp, struct JimExprNode *node)
{

    int result = JimExprGetTermBoolean(interp, node->left);

    if (result == 1) {

        return JimExprEvalTermNode(interp, node->right);
    }
    else if (result == 0) {

        return JimExprEvalTermNode(interp, node->ternary);
    }

    return JIM_ERR;
}

enum
{
    OP_FUNC = 0x0001,
    OP_RIGHT_ASSOC = 0x0002,
};

#define OPRINIT_ATTR(N, P, ARITY, F, ATTR) {N, F, P, ARITY, ATTR, sizeof(N) - 1}
#define OPRINIT(N, P, ARITY, F) OPRINIT_ATTR(N, P, ARITY, F, 0)

static const struct Jim_ExprOperator Jim_ExprOperators[] = {
    OPRINIT("*", 110, 2, JimExprOpBin),
    OPRINIT("/", 110, 2, JimExprOpBin),
    OPRINIT("%", 110, 2, JimExprOpIntBin),

    OPRINIT("-", 100, 2, JimExprOpBin),
    OPRINIT("+", 100, 2, JimExprOpBin),

    OPRINIT("<<", 90, 2, JimExprOpIntBin),
    OPRINIT(">>", 90, 2, JimExprOpIntBin),

    OPRINIT("<<<", 90, 2, JimExprOpIntBin),
    OPRINIT(">>>", 90, 2, JimExprOpIntBin),

    OPRINIT("<", 80, 2, JimExprOpBin),
    OPRINIT(">", 80, 2, JimExprOpBin),
    OPRINIT("<=", 80, 2, JimExprOpBin),
    OPRINIT(">=", 80, 2, JimExprOpBin),

    OPRINIT("==", 70, 2, JimExprOpBin),
    OPRINIT("!=", 70, 2, JimExprOpBin),

    OPRINIT("&", 50, 2, JimExprOpIntBin),
    OPRINIT("^", 49, 2, JimExprOpIntBin),
    OPRINIT("|", 48, 2, JimExprOpIntBin),

    OPRINIT("&&", 10, 2, JimExprOpAnd),
    OPRINIT("||", 9, 2, JimExprOpOr),
    OPRINIT_ATTR("?", 5, 3, JimExprOpTernary, OP_RIGHT_ASSOC),
    OPRINIT_ATTR(":", 5, 3, NULL, OP_RIGHT_ASSOC),


    OPRINIT_ATTR("**", 120, 2, JimExprOpBin, OP_RIGHT_ASSOC),

    OPRINIT("eq", 60, 2, JimExprOpStrBin),
    OPRINIT("ne", 60, 2, JimExprOpStrBin),

    OPRINIT("in", 55, 2, JimExprOpStrBin),
    OPRINIT("ni", 55, 2, JimExprOpStrBin),

    OPRINIT_ATTR("!", 150, 1, JimExprOpNumUnary, OP_RIGHT_ASSOC),
    OPRINIT_ATTR("~", 150, 1, JimExprOpIntUnary, OP_RIGHT_ASSOC),
    OPRINIT_ATTR(" -", 150, 1, JimExprOpNumUnary, OP_RIGHT_ASSOC),
    OPRINIT_ATTR(" +", 150, 1, JimExprOpNumUnary, OP_RIGHT_ASSOC),



    OPRINIT_ATTR("int", 200, 1, JimExprOpNumUnary, OP_FUNC),
    OPRINIT_ATTR("wide", 200, 1, JimExprOpNumUnary, OP_FUNC),
    OPRINIT_ATTR("abs", 200, 1, JimExprOpNumUnary, OP_FUNC),
    OPRINIT_ATTR("double", 200, 1, JimExprOpNumUnary, OP_FUNC),
    OPRINIT_ATTR("round", 200, 1, JimExprOpNumUnary, OP_FUNC),
    OPRINIT_ATTR("rand", 200, 0, JimExprOpNone, OP_FUNC),
    OPRINIT_ATTR("srand", 200, 1, JimExprOpIntUnary, OP_FUNC),

#ifdef JIM_MATH_FUNCTIONS
    OPRINIT_ATTR("sin", 200, 1, JimExprOpDoubleUnary, OP_FUNC),
    OPRINIT_ATTR("cos", 200, 1, JimExprOpDoubleUnary, OP_FUNC),
    OPRINIT_ATTR("tan", 200, 1, JimExprOpDoubleUnary, OP_FUNC),
    OPRINIT_ATTR("asin", 200, 1, JimExprOpDoubleUnary, OP_FUNC),
    OPRINIT_ATTR("acos", 200, 1, JimExprOpDoubleUnary, OP_FUNC),
    OPRINIT_ATTR("atan", 200, 1, JimExprOpDoubleUnary, OP_FUNC),
    OPRINIT_ATTR("atan2", 200, 2, JimExprOpBin, OP_FUNC),
    OPRINIT_ATTR("sinh", 200, 1, JimExprOpDoubleUnary, OP_FUNC),
    OPRINIT_ATTR("cosh", 200, 1, JimExprOpDoubleUnary, OP_FUNC),
    OPRINIT_ATTR("tanh", 200, 1, JimExprOpDoubleUnary, OP_FUNC),
    OPRINIT_ATTR("ceil", 200, 1, JimExprOpDoubleUnary, OP_FUNC),
    OPRINIT_ATTR("floor", 200, 1, JimExprOpDoubleUnary, OP_FUNC),
    OPRINIT_ATTR("exp", 200, 1, JimExprOpDoubleUnary, OP_FUNC),
    OPRINIT_ATTR("log", 200, 1, JimExprOpDoubleUnary, OP_FUNC),
    OPRINIT_ATTR("log10", 200, 1, JimExprOpDoubleUnary, OP_FUNC),
    OPRINIT_ATTR("sqrt", 200, 1, JimExprOpDoubleUnary, OP_FUNC),
    OPRINIT_ATTR("pow", 200, 2, JimExprOpBin, OP_FUNC),
    OPRINIT_ATTR("hypot", 200, 2, JimExprOpBin, OP_FUNC),
    OPRINIT_ATTR("fmod", 200, 2, JimExprOpBin, OP_FUNC),
#endif
};
#undef OPRINIT
#undef OPRINIT_ATTR

#define JIM_EXPR_OPERATORS_NUM \
    (sizeof(Jim_ExprOperators)/sizeof(struct Jim_ExprOperator))

static int JimParseExpression(struct JimParserCtx *pc)
{

    while (isspace(UCHAR(*pc->p)) || (*(pc->p) == '\\' && *(pc->p + 1) == '\n')) {
        if (*pc->p == '\n') {
            pc->linenr++;
        }
        pc->p++;
        pc->len--;
    }


    pc->tline = pc->linenr;
    pc->tstart = pc->p;

    if (pc->len == 0) {
        pc->tend = pc->p;
        pc->tt = JIM_TT_EOL;
        pc->eof = 1;
        return JIM_OK;
    }
    switch (*(pc->p)) {
        case '(':
                pc->tt = JIM_TT_SUBEXPR_START;
                goto singlechar;
        case ')':
                pc->tt = JIM_TT_SUBEXPR_END;
                goto singlechar;
        case ',':
            pc->tt = JIM_TT_SUBEXPR_COMMA;
singlechar:
            pc->tend = pc->p;
            pc->p++;
            pc->len--;
            break;
        case '[':
            return JimParseCmd(pc);
        case '$':
            if (JimParseVar(pc) == JIM_ERR)
                return JimParseExprOperator(pc);
            else {

                if (pc->tt == JIM_TT_EXPRSUGAR) {
                    return JIM_ERR;
                }
                return JIM_OK;
            }
            break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '.':
            return JimParseExprNumber(pc);
        case '"':
            return JimParseQuote(pc);
        case '{':
            return JimParseBrace(pc);

        case 'N':
        case 'I':
        case 'n':
        case 'i':
            if (JimParseExprIrrational(pc) == JIM_ERR)
                if (JimParseExprBoolean(pc) == JIM_ERR)
                    return JimParseExprOperator(pc);
            break;
        case 't':
        case 'f':
        case 'o':
        case 'y':
            if (JimParseExprBoolean(pc) == JIM_ERR)
                return JimParseExprOperator(pc);
            break;
        default:
            return JimParseExprOperator(pc);
            break;
    }
    return JIM_OK;
}

static int JimParseExprNumber(struct JimParserCtx *pc)
{
    char *end;


    pc->tt = JIM_TT_EXPR_INT;

    jim_strtoull(pc->p, (char **)&pc->p);

    if (strchr("eENnIi.", *pc->p) || pc->p == pc->tstart) {
        if (strtod(pc->tstart, &end)) { }
        if (end == pc->tstart)
            return JIM_ERR;
        if (end > pc->p) {

            pc->tt = JIM_TT_EXPR_DOUBLE;
            pc->p = end;
        }
    }
    pc->tend = pc->p - 1;
    pc->len -= (pc->p - pc->tstart);
    return JIM_OK;
}

static int JimParseExprIrrational(struct JimParserCtx *pc)
{
    const char *irrationals[] = { "NaN", "nan", "NAN", "Inf", "inf", "INF", NULL };
    int i;

    for (i = 0; irrationals[i]; i++) {
        const char *irr = irrationals[i];

        if (strncmp(irr, pc->p, 3) == 0) {
            pc->p += 3;
            pc->len -= 3;
            pc->tend = pc->p - 1;
            pc->tt = JIM_TT_EXPR_DOUBLE;
            return JIM_OK;
        }
    }
    return JIM_ERR;
}

static int JimParseExprBoolean(struct JimParserCtx *pc)
{
    const char *booleans[] = { "false", "no", "off", "true", "yes", "on", NULL };
    const int lengths[] = { 5, 2, 3, 4, 3, 2, 0 };
    int i;

    for (i = 0; booleans[i]; i++) {
        const char *boolean = booleans[i];
        int length = lengths[i];

        if (strncmp(boolean, pc->p, length) == 0) {
            pc->p += length;
            pc->len -= length;
            pc->tend = pc->p - 1;
            pc->tt = JIM_TT_EXPR_BOOLEAN;
            return JIM_OK;
        }
    }
    return JIM_ERR;
}

static const struct Jim_ExprOperator *JimExprOperatorInfoByOpcode(int opcode)
{
    static Jim_ExprOperator dummy_op;
    if (opcode < JIM_TT_EXPR_OP) {
        return &dummy_op;
    }
    return &Jim_ExprOperators[opcode - JIM_TT_EXPR_OP];
}

static int JimParseExprOperator(struct JimParserCtx *pc)
{
    int i;
    const struct Jim_ExprOperator *bestOp = NULL;
    int bestLen = 0;


    for (i = 0; i < (signed)JIM_EXPR_OPERATORS_NUM; i++) {
        const struct Jim_ExprOperator *op = &Jim_ExprOperators[i];

        if (op->name[0] != pc->p[0]) {
            continue;
        }

        if (op->namelen > bestLen && strncmp(op->name, pc->p, op->namelen) == 0) {
            bestOp = op;
            bestLen = op->namelen;
        }
    }
    if (bestOp == NULL) {
        return JIM_ERR;
    }


    if (bestOp->attr & OP_FUNC) {
        const char *p = pc->p + bestLen;
        int len = pc->len - bestLen;

        while (len && isspace(UCHAR(*p))) {
            len--;
            p++;
        }
        if (*p != '(') {
            return JIM_ERR;
        }
    }
    pc->tend = pc->p + bestLen - 1;
    pc->p += bestLen;
    pc->len -= bestLen;

    pc->tt = (bestOp - Jim_ExprOperators) + JIM_TT_EXPR_OP;
    return JIM_OK;
}

const char *jim_tt_name(int type)
{
    static const char * const tt_names[JIM_TT_EXPR_OP] =
        { "NIL", "STR", "ESC", "VAR", "ARY", "CMD", "SEP", "EOL", "EOF", "LIN", "WRD", "(((", ")))", ",,,", "INT",
            "DBL", "BOO", "$()" };
    if (type < JIM_TT_EXPR_OP) {
        return tt_names[type];
    }
    else if (type == JIM_EXPROP_UNARYMINUS) {
        return "-VE";
    }
    else if (type == JIM_EXPROP_UNARYPLUS) {
        return "+VE";
    }
    else {
        const struct Jim_ExprOperator *op = JimExprOperatorInfoByOpcode(type);
        static char buf[20];

        if (op->name) {
            return op->name;
        }
        sprintf(buf, "(%d)", type);
        return buf;
    }
}

static void FreeExprInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void DupExprInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);
static int SetExprFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr);

static const Jim_ObjType exprObjType = {
    "expression",
    FreeExprInternalRep,
    DupExprInternalRep,
    NULL,
    JIM_TYPE_REFERENCES,
};


struct ExprTree
{
    struct JimExprNode *expr;
    struct JimExprNode *nodes;
    int len;
    int inUse;
};

static void ExprTreeFreeNodes(Jim_Interp *interp, struct JimExprNode *nodes, int num)
{
    int i;
    for (i = 0; i < num; i++) {
        if (nodes[i].objPtr) {
            Jim_DecrRefCount(interp, nodes[i].objPtr);
        }
    }
    Jim_Free(nodes);
}

static void ExprTreeFree(Jim_Interp *interp, struct ExprTree *expr)
{
    ExprTreeFreeNodes(interp, expr->nodes, expr->len);
    Jim_Free(expr);
}

static void FreeExprInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    struct ExprTree *expr = (void *)objPtr->internalRep.ptr;

    if (expr) {
        if (--expr->inUse != 0) {
            return;
        }

        ExprTreeFree(interp, expr);
    }
}

static void DupExprInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    JIM_NOTUSED(interp);
    JIM_NOTUSED(srcPtr);


    dupPtr->typePtr = NULL;
}

struct ExprBuilder {
    int parencount;
    int level;
    ParseToken *token;
    ParseToken *first_token;
    Jim_Stack stack;
    Jim_Obj *exprObjPtr;
    Jim_Obj *fileNameObj;
    struct JimExprNode *nodes;
    struct JimExprNode *next;
};

#ifdef DEBUG_SHOW_EXPR
static void JimShowExprNode(struct JimExprNode *node, int level)
{
    int i;
    for (i = 0; i < level; i++) {
        printf("  ");
    }
    if (TOKEN_IS_EXPR_OP(node->type)) {
        printf("%s\n", jim_tt_name(node->type));
        if (node->left) {
            JimShowExprNode(node->left, level + 1);
        }
        if (node->right) {
            JimShowExprNode(node->right, level + 1);
        }
        if (node->ternary) {
            JimShowExprNode(node->ternary, level + 1);
        }
    }
    else {
        printf("[%s] %s\n", jim_tt_name(node->type), Jim_String(node->objPtr));
    }
}
#endif

#define EXPR_UNTIL_CLOSE 0x0001
#define EXPR_FUNC_ARGS   0x0002
#define EXPR_TERNARY     0x0004

static int ExprTreeBuildTree(Jim_Interp *interp, struct ExprBuilder *builder, int precedence, int flags, int exp_numterms)
{
    int rc;
    struct JimExprNode *node;

    int exp_stacklen = builder->stack.len + exp_numterms;

    if (builder->level++ > 200) {
        Jim_SetResultString(interp, "Expression too complex", -1);
        return JIM_ERR;
    }

    while (builder->token->type != JIM_TT_EOL) {
        ParseToken *t = builder->token++;
        int prevtt;

        if (t == builder->first_token) {
            prevtt = JIM_TT_NONE;
        }
        else {
            prevtt = t[-1].type;
        }

        if (t->type == JIM_TT_SUBEXPR_START) {
            if (builder->stack.len == exp_stacklen) {
                Jim_SetResultFormatted(interp, "unexpected open parenthesis in expression: \"%#s\"", builder->exprObjPtr);
                return JIM_ERR;
            }
            builder->parencount++;
            rc = ExprTreeBuildTree(interp, builder, 0, EXPR_UNTIL_CLOSE, 1);
            if (rc != JIM_OK) {
                return rc;
            }

        }
        else if (t->type == JIM_TT_SUBEXPR_END) {
            if (!(flags & EXPR_UNTIL_CLOSE)) {
                if (builder->stack.len == exp_stacklen && builder->level > 1) {
                    builder->token--;
                    builder->level--;
                    return JIM_OK;
                }
                Jim_SetResultFormatted(interp, "unexpected closing parenthesis in expression: \"%#s\"", builder->exprObjPtr);
                return JIM_ERR;
            }
            builder->parencount--;
            if (builder->stack.len == exp_stacklen) {

                break;
            }
        }
        else if (t->type == JIM_TT_SUBEXPR_COMMA) {
            if (!(flags & EXPR_FUNC_ARGS)) {
                if (builder->stack.len == exp_stacklen) {

                    builder->token--;
                    builder->level--;
                    return JIM_OK;
                }
                Jim_SetResultFormatted(interp, "unexpected comma in expression: \"%#s\"", builder->exprObjPtr);
                return JIM_ERR;
            }
            else {

                if (builder->stack.len > exp_stacklen) {
                    Jim_SetResultFormatted(interp, "too many arguments to math function");
                    return JIM_ERR;
                }
            }

        }
        else if (t->type == JIM_EXPROP_COLON) {
            if (!(flags & EXPR_TERNARY)) {
                if (builder->level != 1) {

                    builder->token--;
                    builder->level--;
                    return JIM_OK;
                }
                Jim_SetResultFormatted(interp, ": without ? in expression: \"%#s\"", builder->exprObjPtr);
                return JIM_ERR;
            }
            if (builder->stack.len == exp_stacklen) {

                builder->token--;
                builder->level--;
                return JIM_OK;
            }

        }
        else if (TOKEN_IS_EXPR_OP(t->type)) {
            const struct Jim_ExprOperator *op;


            if (TOKEN_IS_EXPR_OP(prevtt) || TOKEN_IS_EXPR_START(prevtt)) {
                if (t->type == JIM_EXPROP_SUB) {
                    t->type = JIM_EXPROP_UNARYMINUS;
                }
                else if (t->type == JIM_EXPROP_ADD) {
                    t->type = JIM_EXPROP_UNARYPLUS;
                }
            }

            op = JimExprOperatorInfoByOpcode(t->type);

            if (op->precedence < precedence || (!(op->attr & OP_RIGHT_ASSOC) && op->precedence == precedence)) {

                builder->token--;
                break;
            }

            if (op->attr & OP_FUNC) {
                if (builder->token->type != JIM_TT_SUBEXPR_START) {
                    Jim_SetResultString(interp, "missing arguments for math function", -1);
                    return JIM_ERR;
                }
                builder->token++;
                if (op->arity == 0) {
                    if (builder->token->type != JIM_TT_SUBEXPR_END) {
                        Jim_SetResultString(interp, "too many arguments for math function", -1);
                        return JIM_ERR;
                    }
                    builder->token++;
                    goto noargs;
                }
                builder->parencount++;


                rc = ExprTreeBuildTree(interp, builder, 0, EXPR_FUNC_ARGS | EXPR_UNTIL_CLOSE, op->arity);
            }
            else if (t->type == JIM_EXPROP_TERNARY) {

                rc = ExprTreeBuildTree(interp, builder, op->precedence, EXPR_TERNARY, 2);
            }
            else {
                rc = ExprTreeBuildTree(interp, builder, op->precedence, 0, 1);
            }

            if (rc != JIM_OK) {
                return rc;
            }

noargs:
            node = builder->next++;
            node->type = t->type;

            if (op->arity >= 3) {
                node->ternary = Jim_StackPop(&builder->stack);
                if (node->ternary == NULL) {
                    goto missingoperand;
                }
            }
            if (op->arity >= 2) {
                node->right = Jim_StackPop(&builder->stack);
                if (node->right == NULL) {
                    goto missingoperand;
                }
            }
            if (op->arity >= 1) {
                node->left = Jim_StackPop(&builder->stack);
                if (node->left == NULL) {
missingoperand:
                    Jim_SetResultFormatted(interp, "missing operand to %s in expression: \"%#s\"", op->name, builder->exprObjPtr);
                    builder->next--;
                    return JIM_ERR;

                }
            }


            Jim_StackPush(&builder->stack, node);
        }
        else {
            Jim_Obj *objPtr = NULL;




            if (!TOKEN_IS_EXPR_START(prevtt) && !TOKEN_IS_EXPR_OP(prevtt)) {
                Jim_SetResultFormatted(interp, "missing operator in expression: \"%#s\"", builder->exprObjPtr);
                return JIM_ERR;
            }


            if (t->type == JIM_TT_EXPR_INT || t->type == JIM_TT_EXPR_DOUBLE) {
                char *endptr;
                if (t->type == JIM_TT_EXPR_INT) {
                    objPtr = Jim_NewIntObj(interp, jim_strtoull(t->token, &endptr));
                }
                else {
                    objPtr = Jim_NewDoubleObj(interp, strtod(t->token, &endptr));
                }
                if (endptr != t->token + t->len) {

                    Jim_FreeNewObj(interp, objPtr);
                    objPtr = NULL;
                }
            }

            if (!objPtr) {

                objPtr = Jim_NewStringObj(interp, t->token, t->len);
                if (t->type == JIM_TT_CMD) {

                    JimSetSourceInfo(interp, objPtr, builder->fileNameObj, t->line);
                }
            }


            node = builder->next++;
            node->objPtr = objPtr;
            Jim_IncrRefCount(node->objPtr);
            node->type = t->type;
            Jim_StackPush(&builder->stack, node);
        }
    }

    if (builder->stack.len == exp_stacklen) {
        builder->level--;
        return JIM_OK;
    }

    if ((flags & EXPR_FUNC_ARGS)) {
        Jim_SetResultFormatted(interp, "too %s arguments for math function", (builder->stack.len < exp_stacklen) ? "few" : "many");
    }
    else {
        if (builder->stack.len < exp_stacklen) {
            if (builder->level == 0) {
                Jim_SetResultFormatted(interp, "empty expression");
            }
            else {
                Jim_SetResultFormatted(interp, "syntax error in expression \"%#s\": premature end of expression", builder->exprObjPtr);
            }
        }
        else {
            Jim_SetResultFormatted(interp, "extra terms after expression");
        }
    }

    return JIM_ERR;
}

static struct ExprTree *ExprTreeCreateTree(Jim_Interp *interp, const ParseTokenList *tokenlist, Jim_Obj *exprObjPtr, Jim_Obj *fileNameObj)
{
    struct ExprTree *expr;
    struct ExprBuilder builder;
    int rc;
    struct JimExprNode *top = NULL;

    builder.parencount = 0;
    builder.level = 0;
    builder.token = builder.first_token = tokenlist->list;
    builder.exprObjPtr = exprObjPtr;
    builder.fileNameObj = fileNameObj;

    builder.nodes = malloc(sizeof(struct JimExprNode) * (tokenlist->count - 1));
    memset(builder.nodes, 0, sizeof(struct JimExprNode) * (tokenlist->count - 1));
    builder.next = builder.nodes;
    Jim_InitStack(&builder.stack);

    rc = ExprTreeBuildTree(interp, &builder, 0, 0, 1);

    if (rc == JIM_OK) {
        top = Jim_StackPop(&builder.stack);

        if (builder.parencount) {
            Jim_SetResultString(interp, "missing close parenthesis", -1);
            rc = JIM_ERR;
        }
    }


    Jim_FreeStack(&builder.stack);

    if (rc != JIM_OK) {
        ExprTreeFreeNodes(interp, builder.nodes, builder.next - builder.nodes);
        return NULL;
    }

    expr = Jim_Alloc(sizeof(*expr));
    expr->inUse = 1;
    expr->expr = top;
    expr->nodes = builder.nodes;
    expr->len = builder.next - builder.nodes;

    assert(expr->len <= tokenlist->count - 1);

    return expr;
}

static int SetExprFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    int exprTextLen;
    const char *exprText;
    struct JimParserCtx parser;
    struct ExprTree *expr;
    ParseTokenList tokenlist;
    int line;
    Jim_Obj *fileNameObj;
    int rc = JIM_ERR;


    if (objPtr->typePtr == &sourceObjType) {
        fileNameObj = objPtr->internalRep.sourceValue.fileNameObj;
        line = objPtr->internalRep.sourceValue.lineNumber;
    }
    else {
        fileNameObj = interp->emptyObj;
        line = 1;
    }
    Jim_IncrRefCount(fileNameObj);

    exprText = Jim_GetString(objPtr, &exprTextLen);


    ScriptTokenListInit(&tokenlist);

    JimParserInit(&parser, exprText, exprTextLen, line);
    while (!parser.eof) {
        if (JimParseExpression(&parser) != JIM_OK) {
            ScriptTokenListFree(&tokenlist);
            Jim_SetResultFormatted(interp, "syntax error in expression: \"%#s\"", objPtr);
            expr = NULL;
            goto err;
        }

        ScriptAddToken(&tokenlist, parser.tstart, parser.tend - parser.tstart + 1, parser.tt,
            parser.tline);
    }

#ifdef DEBUG_SHOW_EXPR_TOKENS
    {
        int i;
        printf("==== Expr Tokens (%s) ====\n", Jim_String(fileNameObj));
        for (i = 0; i < tokenlist.count; i++) {
            printf("[%2d]@%d %s '%.*s'\n", i, tokenlist.list[i].line, jim_tt_name(tokenlist.list[i].type),
                tokenlist.list[i].len, tokenlist.list[i].token);
        }
    }
#endif

    if (JimParseCheckMissing(interp, parser.missing.ch) == JIM_ERR) {
        ScriptTokenListFree(&tokenlist);
        Jim_DecrRefCount(interp, fileNameObj);
        return JIM_ERR;
    }


    expr = ExprTreeCreateTree(interp, &tokenlist, objPtr, fileNameObj);


    ScriptTokenListFree(&tokenlist);

    if (!expr) {
        goto err;
    }

#ifdef DEBUG_SHOW_EXPR
    printf("==== Expr ====\n");
    JimShowExprNode(expr->expr, 0);
#endif

    rc = JIM_OK;

  err:

    Jim_DecrRefCount(interp, fileNameObj);
    Jim_FreeIntRep(interp, objPtr);
    Jim_SetIntRepPtr(objPtr, expr);
    objPtr->typePtr = &exprObjType;
    return rc;
}

static struct ExprTree *JimGetExpression(Jim_Interp *interp, Jim_Obj *objPtr)
{
    if (objPtr->typePtr != &exprObjType) {
        if (SetExprFromAny(interp, objPtr) != JIM_OK) {
            return NULL;
        }
    }
    return (struct ExprTree *) Jim_GetIntRepPtr(objPtr);
}

#ifdef JIM_OPTIMIZATION
static Jim_Obj *JimExprIntValOrVar(Jim_Interp *interp, struct JimExprNode *node)
{
    if (node->type == JIM_TT_EXPR_INT)
        return node->objPtr;
    else if (node->type == JIM_TT_VAR)
        return Jim_GetVariable(interp, node->objPtr, JIM_NONE);
    else if (node->type == JIM_TT_DICTSUGAR)
        return JimExpandDictSugar(interp, node->objPtr);
    else
        return NULL;
}
#endif


static int JimExprEvalTermNode(Jim_Interp *interp, struct JimExprNode *node)
{
    if (TOKEN_IS_EXPR_OP(node->type)) {
        const struct Jim_ExprOperator *op = JimExprOperatorInfoByOpcode(node->type);
        return op->funcop(interp, node);
    }
    else {
        Jim_Obj *objPtr;


        switch (node->type) {
            case JIM_TT_EXPR_INT:
            case JIM_TT_EXPR_DOUBLE:
            case JIM_TT_EXPR_BOOLEAN:
            case JIM_TT_STR:
                Jim_SetResult(interp, node->objPtr);
                return JIM_OK;

            case JIM_TT_VAR:
                objPtr = Jim_GetVariable(interp, node->objPtr, JIM_ERRMSG);
                if (objPtr) {
                    Jim_SetResult(interp, objPtr);
                    return JIM_OK;
                }
                return JIM_ERR;

            case JIM_TT_DICTSUGAR:
                objPtr = JimExpandDictSugar(interp, node->objPtr);
                if (objPtr) {
                    Jim_SetResult(interp, objPtr);
                    return JIM_OK;
                }
                return JIM_ERR;

            case JIM_TT_ESC:
                if (Jim_SubstObj(interp, node->objPtr, &objPtr, JIM_NONE) == JIM_OK) {
                    Jim_SetResult(interp, objPtr);
                    return JIM_OK;
                }
                return JIM_ERR;

            case JIM_TT_CMD:
                return Jim_EvalObj(interp, node->objPtr);

            default:

                return JIM_ERR;
        }
    }
}

static int JimExprGetTerm(Jim_Interp *interp, struct JimExprNode *node, Jim_Obj **objPtrPtr)
{
    int rc = JimExprEvalTermNode(interp, node);
    if (rc == JIM_OK) {
        *objPtrPtr = Jim_GetResult(interp);
        Jim_IncrRefCount(*objPtrPtr);
    }
    return rc;
}

static int JimExprGetTermBoolean(Jim_Interp *interp, struct JimExprNode *node)
{
    if (JimExprEvalTermNode(interp, node) == JIM_OK) {
        return ExprBool(interp, Jim_GetResult(interp));
    }
    return -1;
}

int Jim_EvalExpression(Jim_Interp *interp, Jim_Obj *exprObjPtr)
{
    struct ExprTree *expr;
    int retcode = JIM_OK;

    expr = JimGetExpression(interp, exprObjPtr);
    if (!expr) {
        return JIM_ERR;
    }

#ifdef JIM_OPTIMIZATION
    {
        Jim_Obj *objPtr;


        switch (expr->len) {
            case 1:
                objPtr = JimExprIntValOrVar(interp, expr->expr);
                if (objPtr) {
                    Jim_SetResult(interp, objPtr);
                    return JIM_OK;
                }
                break;

            case 2:
                if (expr->expr->type == JIM_EXPROP_NOT) {
                    objPtr = JimExprIntValOrVar(interp, expr->expr->left);

                    if (objPtr && JimIsWide(objPtr)) {
                        Jim_SetResult(interp, JimWideValue(objPtr) ? interp->falseObj : interp->trueObj);
                        return JIM_OK;
                    }
                }
                break;

            case 3:
                objPtr = JimExprIntValOrVar(interp, expr->expr->left);
                if (objPtr && JimIsWide(objPtr)) {
                    Jim_Obj *objPtr2 = JimExprIntValOrVar(interp, expr->expr->right);
                    if (objPtr2 && JimIsWide(objPtr2)) {
                        jim_wide wideValueA = JimWideValue(objPtr);
                        jim_wide wideValueB = JimWideValue(objPtr2);
                        int cmpRes;
                        switch (expr->expr->type) {
                            case JIM_EXPROP_LT:
                                cmpRes = wideValueA < wideValueB;
                                break;
                            case JIM_EXPROP_LTE:
                                cmpRes = wideValueA <= wideValueB;
                                break;
                            case JIM_EXPROP_GT:
                                cmpRes = wideValueA > wideValueB;
                                break;
                            case JIM_EXPROP_GTE:
                                cmpRes = wideValueA >= wideValueB;
                                break;
                            case JIM_EXPROP_NUMEQ:
                                cmpRes = wideValueA == wideValueB;
                                break;
                            case JIM_EXPROP_NUMNE:
                                cmpRes = wideValueA != wideValueB;
                                break;
                            default:
                                goto noopt;
                        }
                        Jim_SetResult(interp, cmpRes ? interp->trueObj : interp->falseObj);
                        return JIM_OK;
                    }
                }
                break;
        }
    }
noopt:
#endif

    expr->inUse++;


    retcode = JimExprEvalTermNode(interp, expr->expr);

    expr->inUse--;

    return retcode;
}

int Jim_GetBoolFromExpr(Jim_Interp *interp, Jim_Obj *exprObjPtr, int *boolPtr)
{
    int retcode = Jim_EvalExpression(interp, exprObjPtr);

    if (retcode == JIM_OK) {
        switch (ExprBool(interp, Jim_GetResult(interp))) {
            case 0:
                *boolPtr = 0;
                break;

            case 1:
                *boolPtr = 1;
                break;

            case -1:
                retcode = JIM_ERR;
                break;
        }
    }
    return retcode;
}




typedef struct ScanFmtPartDescr
{
    const char *arg;
    const char *prefix;
    size_t width;
    int pos;
    char type;
    char modifier;
} ScanFmtPartDescr;


typedef struct ScanFmtStringObj
{
    jim_wide size;
    char *stringRep;
    size_t count;
    size_t convCount;
    size_t maxPos;
    const char *error;
    char *scratch;
    ScanFmtPartDescr descr[1];
} ScanFmtStringObj;


static void FreeScanFmtInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void DupScanFmtInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);
static void UpdateStringOfScanFmt(Jim_Obj *objPtr);

static const Jim_ObjType scanFmtStringObjType = {
    "scanformatstring",
    FreeScanFmtInternalRep,
    DupScanFmtInternalRep,
    UpdateStringOfScanFmt,
    JIM_TYPE_NONE,
};

void FreeScanFmtInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    JIM_NOTUSED(interp);
    Jim_Free((char *)objPtr->internalRep.ptr);
    objPtr->internalRep.ptr = 0;
}

void DupScanFmtInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    size_t size = (size_t) ((ScanFmtStringObj *) srcPtr->internalRep.ptr)->size;
    ScanFmtStringObj *newVec = (ScanFmtStringObj *) Jim_Alloc(size);

    JIM_NOTUSED(interp);
    memcpy(newVec, srcPtr->internalRep.ptr, size);
    dupPtr->internalRep.ptr = newVec;
    dupPtr->typePtr = &scanFmtStringObjType;
}

static void UpdateStringOfScanFmt(Jim_Obj *objPtr)
{
    JimSetStringBytes(objPtr, ((ScanFmtStringObj *) objPtr->internalRep.ptr)->stringRep);
}


static int SetScanFmtFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    ScanFmtStringObj *fmtObj;
    char *buffer;
    int maxCount, i, approxSize, lastPos = -1;
    const char *fmt = Jim_String(objPtr);
    int maxFmtLen = Jim_Length(objPtr);
    const char *fmtEnd = fmt + maxFmtLen;
    int curr;

    Jim_FreeIntRep(interp, objPtr);

    for (i = 0, maxCount = 0; i < maxFmtLen; ++i)
        if (fmt[i] == '%')
            ++maxCount;

    approxSize = sizeof(ScanFmtStringObj)
        +(maxCount + 1) * sizeof(ScanFmtPartDescr)
        +maxFmtLen * sizeof(char) + 3 + 1
        + maxFmtLen * sizeof(char) + 1
        + maxFmtLen * sizeof(char)
        +(maxCount + 1) * sizeof(char)
        +1;
    fmtObj = (ScanFmtStringObj *) Jim_Alloc(approxSize);
    memset(fmtObj, 0, approxSize);
    fmtObj->size = approxSize;
    fmtObj->maxPos = 0;
    fmtObj->scratch = (char *)&fmtObj->descr[maxCount + 1];
    fmtObj->stringRep = fmtObj->scratch + maxFmtLen + 3 + 1;
    memcpy(fmtObj->stringRep, fmt, maxFmtLen);
    buffer = fmtObj->stringRep + maxFmtLen + 1;
    objPtr->internalRep.ptr = fmtObj;
    objPtr->typePtr = &scanFmtStringObjType;
    for (i = 0, curr = 0; fmt < fmtEnd; ++fmt) {
        int width = 0, skip;
        ScanFmtPartDescr *descr = &fmtObj->descr[curr];

        fmtObj->count++;
        descr->width = 0;

        if (*fmt != '%' || fmt[1] == '%') {
            descr->type = 0;
            descr->prefix = &buffer[i];
            for (; fmt < fmtEnd; ++fmt) {
                if (*fmt == '%') {
                    if (fmt[1] != '%')
                        break;
                    ++fmt;
                }
                buffer[i++] = *fmt;
            }
            buffer[i++] = 0;
        }

        ++fmt;

        if (fmt >= fmtEnd)
            goto done;
        descr->pos = 0;
        if (*fmt == '*') {
            descr->pos = -1;
            ++fmt;
        }
        else
            fmtObj->convCount++;

        if (sscanf(fmt, "%d%n", &width, &skip) == 1) {
            fmt += skip;

            if (descr->pos != -1 && *fmt == '$') {
                int prev;

                ++fmt;
                descr->pos = width;
                width = 0;

                if ((lastPos == 0 && descr->pos > 0)
                    || (lastPos > 0 && descr->pos == 0)) {
                    fmtObj->error = "cannot mix \"%\" and \"%n$\" conversion specifiers";
                    return JIM_ERR;
                }

                for (prev = 0; prev < curr; ++prev) {
                    if (fmtObj->descr[prev].pos == -1)
                        continue;
                    if (fmtObj->descr[prev].pos == descr->pos) {
                        fmtObj->error =
                            "variable is assigned by multiple \"%n$\" conversion specifiers";
                        return JIM_ERR;
                    }
                }
                if (descr->pos < 0) {
                    fmtObj->error =
                        "\"%n$\" conversion specifier is negative";
                    return JIM_ERR;
                }

                if (sscanf(fmt, "%d%n", &width, &skip) == 1) {
                    descr->width = width;
                    fmt += skip;
                }
                if (descr->pos > 0 && (size_t) descr->pos > fmtObj->maxPos)
                    fmtObj->maxPos = descr->pos;
            }
            else {

                descr->width = width;
            }
        }

        if (lastPos == -1)
            lastPos = descr->pos;

        if (*fmt == '[') {
            int swapped = 1, beg = i, end, j;

            descr->type = '[';
            descr->arg = &buffer[i];
            ++fmt;
            if (*fmt == '^')
                buffer[i++] = *fmt++;
            if (*fmt == ']')
                buffer[i++] = *fmt++;
            while (*fmt && *fmt != ']')
                buffer[i++] = *fmt++;
            if (*fmt != ']') {
                fmtObj->error = "unmatched [ in format string";
                return JIM_ERR;
            }
            end = i;
            buffer[i++] = 0;

            while (swapped) {
                swapped = 0;
                for (j = beg + 1; j < end - 1; ++j) {
                    if (buffer[j] == '-' && buffer[j - 1] > buffer[j + 1]) {
                        char tmp = buffer[j - 1];

                        buffer[j - 1] = buffer[j + 1];
                        buffer[j + 1] = tmp;
                        swapped = 1;
                    }
                }
            }
        }
        else {

            if (fmt < fmtEnd && strchr("hlL", *fmt))
                descr->modifier = tolower((int)*fmt++);

            if (fmt >= fmtEnd) {
                fmtObj->error = "missing scan conversion character";
                return JIM_ERR;
            }

            descr->type = *fmt;
            if (strchr("efgcsndoxui", *fmt) == 0) {
                fmtObj->error = "bad scan conversion character";
                return JIM_ERR;
            }
            else if (*fmt == 'c' && descr->width != 0) {
                fmtObj->error = "field width may not be specified in %c " "conversion";
                return JIM_ERR;
            }
            else if (*fmt == 'u' && descr->modifier == 'l') {
                fmtObj->error = "unsigned wide not supported";
                return JIM_ERR;
            }
        }
        curr++;
    }
  done:
    return JIM_OK;
}



#define FormatGetCnvCount(_fo_) \
    ((ScanFmtStringObj*)((_fo_)->internalRep.ptr))->convCount
#define FormatGetMaxPos(_fo_) \
    ((ScanFmtStringObj*)((_fo_)->internalRep.ptr))->maxPos
#define FormatGetError(_fo_) \
    ((ScanFmtStringObj*)((_fo_)->internalRep.ptr))->error

static Jim_Obj *JimScanAString(Jim_Interp *interp, const char *sdescr, const char *str)
{
    char *buffer = Jim_StrDup(str);
    char *p = buffer;

    while (*str) {
        int c;
        int n;

        if (!sdescr && isspace(UCHAR(*str)))
            break;

        n = utf8_tounicode(str, &c);
        if (sdescr && !JimCharsetMatch(sdescr, c, JIM_CHARSET_SCAN))
            break;
        while (n--)
            *p++ = *str++;
    }
    *p = 0;
    return Jim_NewStringObjNoAlloc(interp, buffer, p - buffer);
}


static int ScanOneEntry(Jim_Interp *interp, const char *str, int pos, int strLen,
    ScanFmtStringObj * fmtObj, long idx, Jim_Obj **valObjPtr)
{
    const char *tok;
    const ScanFmtPartDescr *descr = &fmtObj->descr[idx];
    size_t scanned = 0;
    size_t anchor = pos;
    int i;
    Jim_Obj *tmpObj = NULL;


    *valObjPtr = 0;
    if (descr->prefix) {
        for (i = 0; pos < strLen && descr->prefix[i]; ++i) {

            if (isspace(UCHAR(descr->prefix[i])))
                while (pos < strLen && isspace(UCHAR(str[pos])))
                    ++pos;
            else if (descr->prefix[i] != str[pos])
                break;
            else
                ++pos;
        }
        if (pos >= strLen) {
            return -1;
        }
        else if (descr->prefix[i] != 0)
            return 0;
    }

    if (descr->type != 'c' && descr->type != '[' && descr->type != 'n')
        while (isspace(UCHAR(str[pos])))
            ++pos;

    scanned = pos - anchor;


    if (descr->type == 'n') {

        *valObjPtr = Jim_NewIntObj(interp, anchor + scanned);
    }
    else if (pos >= strLen) {

        return -1;
    }
    else if (descr->type == 'c') {
            int c;
            scanned += utf8_tounicode(&str[pos], &c);
            *valObjPtr = Jim_NewIntObj(interp, c);
            return scanned;
    }
    else {

        if (descr->width > 0) {
            size_t sLen = utf8_strlen(&str[pos], strLen - pos);
            size_t tLen = descr->width > sLen ? sLen : descr->width;

            tmpObj = Jim_NewStringObjUtf8(interp, str + pos, tLen);
            tok = tmpObj->bytes;
        }
        else {

            tok = &str[pos];
        }
        switch (descr->type) {
            case 'd':
            case 'o':
            case 'x':
            case 'u':
            case 'i':{
                    char *endp;
                    jim_wide w;

                    int base = descr->type == 'o' ? 8
                        : descr->type == 'x' ? 16 : descr->type == 'i' ? 0 : 10;


                    if (base == 0) {
                        w = jim_strtoull(tok, &endp);
                    }
                    else {
                        w = strtoull(tok, &endp, base);
                    }

                    if (endp != tok) {

                        *valObjPtr = Jim_NewIntObj(interp, w);


                        scanned += endp - tok;
                    }
                    else {
                        scanned = *tok ? 0 : -1;
                    }
                    break;
                }
            case 's':
            case '[':{
                    *valObjPtr = JimScanAString(interp, descr->arg, tok);
                    scanned += Jim_Length(*valObjPtr);
                    break;
                }
            case 'e':
            case 'f':
            case 'g':{
                    char *endp;
                    double value = strtod(tok, &endp);

                    if (endp != tok) {

                        *valObjPtr = Jim_NewDoubleObj(interp, value);

                        scanned += endp - tok;
                    }
                    else {
                        scanned = *tok ? 0 : -1;
                    }
                    break;
                }
        }
        if (tmpObj) {
            Jim_FreeNewObj(interp, tmpObj);
        }
    }
    return scanned;
}


Jim_Obj *Jim_ScanString(Jim_Interp *interp, Jim_Obj *strObjPtr, Jim_Obj *fmtObjPtr, int flags)
{
    size_t i, pos;
    int scanned = 1;
    const char *str = Jim_String(strObjPtr);
    int strLen = Jim_Utf8Length(interp, strObjPtr);
    Jim_Obj *resultList = 0;
    Jim_Obj **resultVec = 0;
    int resultc;
    Jim_Obj *emptyStr = 0;
    ScanFmtStringObj *fmtObj;


    JimPanic((fmtObjPtr->typePtr != &scanFmtStringObjType, "Jim_ScanString() for non-scan format"));

    fmtObj = (ScanFmtStringObj *) fmtObjPtr->internalRep.ptr;

    if (fmtObj->error != 0) {
        if (flags & JIM_ERRMSG)
            Jim_SetResultString(interp, fmtObj->error, -1);
        return 0;
    }

    emptyStr = Jim_NewEmptyStringObj(interp);
    Jim_IncrRefCount(emptyStr);

    resultList = Jim_NewListObj(interp, NULL, 0);
    if (fmtObj->maxPos > 0) {
        for (i = 0; i < fmtObj->maxPos; ++i)
            Jim_ListAppendElement(interp, resultList, emptyStr);
        JimListGetElements(interp, resultList, &resultc, &resultVec);
    }

    for (i = 0, pos = 0; i < fmtObj->count; ++i) {
        ScanFmtPartDescr *descr = &(fmtObj->descr[i]);
        Jim_Obj *value = 0;


        if (descr->type == 0)
            continue;

        if (scanned > 0)
            scanned = ScanOneEntry(interp, str, pos, strLen, fmtObj, i, &value);

        if (scanned == -1 && i == 0)
            goto eof;

        pos += scanned;


        if (value == 0)
            value = Jim_NewEmptyStringObj(interp);

        if (descr->pos == -1) {
            Jim_FreeNewObj(interp, value);
        }
        else if (descr->pos == 0)

            Jim_ListAppendElement(interp, resultList, value);
        else if (resultVec[descr->pos - 1] == emptyStr) {

            Jim_DecrRefCount(interp, resultVec[descr->pos - 1]);
            Jim_IncrRefCount(value);
            resultVec[descr->pos - 1] = value;
        }
        else {

            Jim_FreeNewObj(interp, value);
            goto err;
        }
    }
    Jim_DecrRefCount(interp, emptyStr);
    return resultList;
  eof:
    Jim_DecrRefCount(interp, emptyStr);
    Jim_FreeNewObj(interp, resultList);
    return (Jim_Obj *)EOF;
  err:
    Jim_DecrRefCount(interp, emptyStr);
    Jim_FreeNewObj(interp, resultList);
    return 0;
}


static void JimPrngInit(Jim_Interp *interp)
{
#define PRNG_SEED_SIZE 256
    int i;
    unsigned int *seed;
    time_t t = time(NULL);

    interp->prngState = Jim_Alloc(sizeof(Jim_PrngState));

    seed = Jim_Alloc(PRNG_SEED_SIZE * sizeof(*seed));
    for (i = 0; i < PRNG_SEED_SIZE; i++) {
        seed[i] = (rand() ^ t ^ clock());
    }
    JimPrngSeed(interp, (unsigned char *)seed, PRNG_SEED_SIZE * sizeof(*seed));
    Jim_Free(seed);
}


static void JimRandomBytes(Jim_Interp *interp, void *dest, unsigned int len)
{
    Jim_PrngState *prng;
    unsigned char *destByte = (unsigned char *)dest;
    unsigned int si, sj, x;


    if (interp->prngState == NULL)
        JimPrngInit(interp);
    prng = interp->prngState;

    for (x = 0; x < len; x++) {
        prng->i = (prng->i + 1) & 0xff;
        si = prng->sbox[prng->i];
        prng->j = (prng->j + si) & 0xff;
        sj = prng->sbox[prng->j];
        prng->sbox[prng->i] = sj;
        prng->sbox[prng->j] = si;
        *destByte++ = prng->sbox[(si + sj) & 0xff];
    }
}


static void JimPrngSeed(Jim_Interp *interp, unsigned char *seed, int seedLen)
{
    int i;
    Jim_PrngState *prng;


    if (interp->prngState == NULL)
        JimPrngInit(interp);
    prng = interp->prngState;


    for (i = 0; i < 256; i++)
        prng->sbox[i] = i;

    for (i = 0; i < seedLen; i++) {
        unsigned char t;

        t = prng->sbox[i & 0xFF];
        prng->sbox[i & 0xFF] = prng->sbox[seed[i]];
        prng->sbox[seed[i]] = t;
    }
    prng->i = prng->j = 0;

    for (i = 0; i < 256; i += seedLen) {
        JimRandomBytes(interp, seed, seedLen);
    }
}


static int Jim_IncrCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_wide wideValue, increment = 1;
    Jim_Obj *intObjPtr;

    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "varName ?increment?");
        return JIM_ERR;
    }
    if (argc == 3) {
        if (Jim_GetWide(interp, argv[2], &increment) != JIM_OK)
            return JIM_ERR;
    }
    intObjPtr = Jim_GetVariable(interp, argv[1], JIM_UNSHARED);
    if (!intObjPtr) {

        wideValue = 0;
    }
    else if (Jim_GetWide(interp, intObjPtr, &wideValue) != JIM_OK) {
        return JIM_ERR;
    }
    if (!intObjPtr || Jim_IsShared(intObjPtr)) {
        intObjPtr = Jim_NewIntObj(interp, wideValue + increment);
        if (Jim_SetVariable(interp, argv[1], intObjPtr) != JIM_OK) {
            Jim_FreeNewObj(interp, intObjPtr);
            return JIM_ERR;
        }
    }
    else {

        Jim_InvalidateStringRep(intObjPtr);
        JimWideValue(intObjPtr) = wideValue + increment;

        if (argv[1]->typePtr != &variableObjType) {

            Jim_SetVariable(interp, argv[1], intObjPtr);
        }
    }
    Jim_SetResult(interp, intObjPtr);
    return JIM_OK;
}


#define JIM_EVAL_SARGV_LEN 8
#define JIM_EVAL_SINTV_LEN 8


static int JimUnknown(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int retcode;

    if (interp->unknown_called > 50) {
        return JIM_ERR;
    }



    if (Jim_GetCommand(interp, interp->unknown, JIM_NONE) == NULL)
        return JIM_ERR;

    interp->unknown_called++;

    retcode = Jim_EvalObjPrefix(interp, interp->unknown, argc, argv);
    interp->unknown_called--;

    return retcode;
}

static int JimInvokeCommand(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    int retcode;
    Jim_Cmd *cmdPtr;
    void *prevPrivData;

#if 0
    printf("invoke");
    int j;
    for (j = 0; j < objc; j++) {
        printf(" '%s'", Jim_String(objv[j]));
    }
    printf("\n");
#endif

    if (interp->framePtr->tailcallCmd) {

        cmdPtr = interp->framePtr->tailcallCmd;
        interp->framePtr->tailcallCmd = NULL;
    }
    else {
        cmdPtr = Jim_GetCommand(interp, objv[0], JIM_ERRMSG);
        if (cmdPtr == NULL) {
            return JimUnknown(interp, objc, objv);
        }
        JimIncrCmdRefCount(cmdPtr);
    }

    if (interp->evalDepth == interp->maxEvalDepth) {
        Jim_SetResultString(interp, "Infinite eval recursion", -1);
        retcode = JIM_ERR;
        goto out;
    }
    interp->evalDepth++;
    prevPrivData = interp->cmdPrivData;


    Jim_SetEmptyResult(interp);
    if (cmdPtr->isproc) {
        retcode = JimCallProcedure(interp, cmdPtr, objc, objv);
    }
    else {
        interp->cmdPrivData = cmdPtr->u.native.privData;
        retcode = cmdPtr->u.native.cmdProc(interp, objc, objv);
    }
    interp->cmdPrivData = prevPrivData;
    interp->evalDepth--;

out:
    JimDecrCmdRefCount(interp, cmdPtr);

    return retcode;
}

int Jim_EvalObjVector(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    int i, retcode;


    for (i = 0; i < objc; i++)
        Jim_IncrRefCount(objv[i]);

    retcode = JimInvokeCommand(interp, objc, objv);


    for (i = 0; i < objc; i++)
        Jim_DecrRefCount(interp, objv[i]);

    return retcode;
}

int Jim_EvalObjPrefix(Jim_Interp *interp, Jim_Obj *prefix, int objc, Jim_Obj *const *objv)
{
    int ret;
    Jim_Obj **nargv = Jim_Alloc((objc + 1) * sizeof(*nargv));

    nargv[0] = prefix;
    memcpy(&nargv[1], &objv[0], sizeof(nargv[0]) * objc);
    ret = Jim_EvalObjVector(interp, objc + 1, nargv);
    Jim_Free(nargv);
    return ret;
}

static void JimAddErrorToStack(Jim_Interp *interp, ScriptObj *script)
{
    if (!interp->errorFlag) {

        interp->errorFlag = 1;
        Jim_IncrRefCount(script->fileNameObj);
        Jim_DecrRefCount(interp, interp->errorFileNameObj);
        interp->errorFileNameObj = script->fileNameObj;
        interp->errorLine = script->linenr;

        JimResetStackTrace(interp);

        interp->addStackTrace++;
    }


    if (interp->addStackTrace > 0) {


        JimAppendStackTrace(interp, Jim_String(interp->errorProc), script->fileNameObj, script->linenr);

        if (Jim_Length(script->fileNameObj)) {
            interp->addStackTrace = 0;
        }

        Jim_DecrRefCount(interp, interp->errorProc);
        interp->errorProc = interp->emptyObj;
        Jim_IncrRefCount(interp->errorProc);
    }
}

static int JimSubstOneToken(Jim_Interp *interp, const ScriptToken *token, Jim_Obj **objPtrPtr)
{
    Jim_Obj *objPtr;

    switch (token->type) {
        case JIM_TT_STR:
        case JIM_TT_ESC:
            objPtr = token->objPtr;
            break;
        case JIM_TT_VAR:
            objPtr = Jim_GetVariable(interp, token->objPtr, JIM_ERRMSG);
            break;
        case JIM_TT_DICTSUGAR:
            objPtr = JimExpandDictSugar(interp, token->objPtr);
            break;
        case JIM_TT_EXPRSUGAR:
            objPtr = JimExpandExprSugar(interp, token->objPtr);
            break;
        case JIM_TT_CMD:
            switch (Jim_EvalObj(interp, token->objPtr)) {
                case JIM_OK:
                case JIM_RETURN:
                    objPtr = interp->result;
                    break;
                case JIM_BREAK:

                    return JIM_BREAK;
                case JIM_CONTINUE:

                    return JIM_CONTINUE;
                default:
                    return JIM_ERR;
            }
            break;
        default:
            JimPanic((1,
                "default token type (%d) reached " "in Jim_SubstObj().", token->type));
            objPtr = NULL;
            break;
    }
    if (objPtr) {
        *objPtrPtr = objPtr;
        return JIM_OK;
    }
    return JIM_ERR;
}

static Jim_Obj *JimInterpolateTokens(Jim_Interp *interp, const ScriptToken * token, int tokens, int flags)
{
    int totlen = 0, i;
    Jim_Obj **intv;
    Jim_Obj *sintv[JIM_EVAL_SINTV_LEN];
    Jim_Obj *objPtr;
    char *s;

    if (tokens <= JIM_EVAL_SINTV_LEN)
        intv = sintv;
    else
        intv = Jim_Alloc(sizeof(Jim_Obj *) * tokens);

    for (i = 0; i < tokens; i++) {
        switch (JimSubstOneToken(interp, &token[i], &intv[i])) {
            case JIM_OK:
            case JIM_RETURN:
                break;
            case JIM_BREAK:
                if (flags & JIM_SUBST_FLAG) {

                    tokens = i;
                    continue;
                }


            case JIM_CONTINUE:
                if (flags & JIM_SUBST_FLAG) {
                    intv[i] = NULL;
                    continue;
                }


            default:
                while (i--) {
                    Jim_DecrRefCount(interp, intv[i]);
                }
                if (intv != sintv) {
                    Jim_Free(intv);
                }
                return NULL;
        }
        Jim_IncrRefCount(intv[i]);
        Jim_String(intv[i]);
        totlen += intv[i]->length;
    }


    if (tokens == 1 && intv[0] && intv == sintv) {

        intv[0]->refCount--;
        return intv[0];
    }

    objPtr = Jim_NewStringObjNoAlloc(interp, NULL, 0);

    if (tokens == 4 && token[0].type == JIM_TT_ESC && token[1].type == JIM_TT_ESC
        && token[2].type == JIM_TT_VAR) {

        objPtr->typePtr = &interpolatedObjType;
        objPtr->internalRep.dictSubstValue.varNameObjPtr = token[0].objPtr;
        objPtr->internalRep.dictSubstValue.indexObjPtr = intv[2];
        Jim_IncrRefCount(intv[2]);
    }
    else if (tokens && intv[0] && intv[0]->typePtr == &sourceObjType) {

        JimSetSourceInfo(interp, objPtr, intv[0]->internalRep.sourceValue.fileNameObj, intv[0]->internalRep.sourceValue.lineNumber);
    }


    s = objPtr->bytes = Jim_Alloc(totlen + 1);
    objPtr->length = totlen;
    for (i = 0; i < tokens; i++) {
        if (intv[i]) {
            memcpy(s, intv[i]->bytes, intv[i]->length);
            s += intv[i]->length;
            Jim_DecrRefCount(interp, intv[i]);
        }
    }
    objPtr->bytes[totlen] = '\0';

    if (intv != sintv) {
        Jim_Free(intv);
    }

    return objPtr;
}


static int JimEvalObjList(Jim_Interp *interp, Jim_Obj *listPtr)
{
    int retcode = JIM_OK;

    JimPanic((Jim_IsList(listPtr) == 0, "JimEvalObjList() invoked on non-list."));

    if (listPtr->internalRep.listValue.len) {
        Jim_IncrRefCount(listPtr);
        retcode = JimInvokeCommand(interp,
            listPtr->internalRep.listValue.len,
            listPtr->internalRep.listValue.ele);
        Jim_DecrRefCount(interp, listPtr);
    }
    return retcode;
}

int Jim_EvalObjList(Jim_Interp *interp, Jim_Obj *listPtr)
{
    SetListFromAny(interp, listPtr);
    return JimEvalObjList(interp, listPtr);
}

int Jim_EvalObj(Jim_Interp *interp, Jim_Obj *scriptObjPtr)
{
    int i;
    ScriptObj *script;
    ScriptToken *token;
    int retcode = JIM_OK;
    Jim_Obj *sargv[JIM_EVAL_SARGV_LEN], **argv = NULL;
    Jim_Obj *prevScriptObj;

    if (Jim_IsList(scriptObjPtr) && scriptObjPtr->bytes == NULL) {
        return JimEvalObjList(interp, scriptObjPtr);
    }

    Jim_IncrRefCount(scriptObjPtr);
    script = JimGetScript(interp, scriptObjPtr);
    if (!JimScriptValid(interp, script)) {
        Jim_DecrRefCount(interp, scriptObjPtr);
        return JIM_ERR;
    }

    Jim_SetEmptyResult(interp);

    token = script->token;

#ifdef JIM_OPTIMIZATION
    if (script->len == 0) {
        Jim_DecrRefCount(interp, scriptObjPtr);
        return JIM_OK;
    }
    if (script->len == 3
        && token[1].objPtr->typePtr == &commandObjType
        && token[1].objPtr->internalRep.cmdValue.cmdPtr->isproc == 0
        && token[1].objPtr->internalRep.cmdValue.cmdPtr->u.native.cmdProc == Jim_IncrCoreCommand
        && token[2].objPtr->typePtr == &variableObjType) {

        Jim_Obj *objPtr = Jim_GetVariable(interp, token[2].objPtr, JIM_NONE);

        if (objPtr && !Jim_IsShared(objPtr) && objPtr->typePtr == &intObjType) {
            JimWideValue(objPtr)++;
            Jim_InvalidateStringRep(objPtr);
            Jim_DecrRefCount(interp, scriptObjPtr);
            Jim_SetResult(interp, objPtr);
            return JIM_OK;
        }
    }
#endif

    script->inUse++;


    prevScriptObj = interp->currentScriptObj;
    interp->currentScriptObj = scriptObjPtr;

    interp->errorFlag = 0;
    argv = sargv;

    for (i = 0; i < script->len && retcode == JIM_OK; ) {
        int argc;
        int j;


        argc = token[i].objPtr->internalRep.scriptLineValue.argc;
        script->linenr = token[i].objPtr->internalRep.scriptLineValue.line;


        if (argc > JIM_EVAL_SARGV_LEN)
            argv = Jim_Alloc(sizeof(Jim_Obj *) * argc);


        i++;

        for (j = 0; j < argc; j++) {
            long wordtokens = 1;
            int expand = 0;
            Jim_Obj *wordObjPtr = NULL;

            if (token[i].type == JIM_TT_WORD) {
                wordtokens = JimWideValue(token[i++].objPtr);
                if (wordtokens < 0) {
                    expand = 1;
                    wordtokens = -wordtokens;
                }
            }

            if (wordtokens == 1) {

                switch (token[i].type) {
                    case JIM_TT_ESC:
                    case JIM_TT_STR:
                        wordObjPtr = token[i].objPtr;
                        break;
                    case JIM_TT_VAR:
                        wordObjPtr = Jim_GetVariable(interp, token[i].objPtr, JIM_ERRMSG);
                        break;
                    case JIM_TT_EXPRSUGAR:
                        wordObjPtr = JimExpandExprSugar(interp, token[i].objPtr);
                        break;
                    case JIM_TT_DICTSUGAR:
                        wordObjPtr = JimExpandDictSugar(interp, token[i].objPtr);
                        break;
                    case JIM_TT_CMD:
                        retcode = Jim_EvalObj(interp, token[i].objPtr);
                        if (retcode == JIM_OK) {
                            wordObjPtr = Jim_GetResult(interp);
                        }
                        break;
                    default:
                        JimPanic((1, "default token type reached " "in Jim_EvalObj()."));
                }
            }
            else {
                wordObjPtr = JimInterpolateTokens(interp, token + i, wordtokens, JIM_NONE);
            }

            if (!wordObjPtr) {
                if (retcode == JIM_OK) {
                    retcode = JIM_ERR;
                }
                break;
            }

            Jim_IncrRefCount(wordObjPtr);
            i += wordtokens;

            if (!expand) {
                argv[j] = wordObjPtr;
            }
            else {

                int len = Jim_ListLength(interp, wordObjPtr);
                int newargc = argc + len - 1;
                int k;

                if (len > 1) {
                    if (argv == sargv) {
                        if (newargc > JIM_EVAL_SARGV_LEN) {
                            argv = Jim_Alloc(sizeof(*argv) * newargc);
                            memcpy(argv, sargv, sizeof(*argv) * j);
                        }
                    }
                    else {

                        argv = Jim_Realloc(argv, sizeof(*argv) * newargc);
                    }
                }


                for (k = 0; k < len; k++) {
                    argv[j++] = wordObjPtr->internalRep.listValue.ele[k];
                    Jim_IncrRefCount(wordObjPtr->internalRep.listValue.ele[k]);
                }

                Jim_DecrRefCount(interp, wordObjPtr);


                j--;
                argc += len - 1;
            }
        }

        if (retcode == JIM_OK && argc) {

            retcode = JimInvokeCommand(interp, argc, argv);

            if (Jim_CheckSignal(interp)) {
                retcode = JIM_SIGNAL;
            }
        }


        while (j-- > 0) {
            Jim_DecrRefCount(interp, argv[j]);
        }

        if (argv != sargv) {
            Jim_Free(argv);
            argv = sargv;
        }
    }


    if (retcode == JIM_ERR) {
        JimAddErrorToStack(interp, script);
    }

    else if (retcode != JIM_RETURN || interp->returnCode != JIM_ERR) {

        interp->addStackTrace = 0;
    }


    interp->currentScriptObj = prevScriptObj;

    Jim_FreeIntRep(interp, scriptObjPtr);
    scriptObjPtr->typePtr = &scriptObjType;
    Jim_SetIntRepPtr(scriptObjPtr, script);
    Jim_DecrRefCount(interp, scriptObjPtr);

    return retcode;
}

static int JimSetProcArg(Jim_Interp *interp, Jim_Obj *argNameObj, Jim_Obj *argValObj)
{
    int retcode;

    const char *varname = Jim_String(argNameObj);
    if (*varname == '&') {

        Jim_Obj *objPtr;
        Jim_CallFrame *savedCallFrame = interp->framePtr;

        interp->framePtr = interp->framePtr->parent;
        objPtr = Jim_GetVariable(interp, argValObj, JIM_ERRMSG);
        interp->framePtr = savedCallFrame;
        if (!objPtr) {
            return JIM_ERR;
        }


        objPtr = Jim_NewStringObj(interp, varname + 1, -1);
        Jim_IncrRefCount(objPtr);
        retcode = Jim_SetVariableLink(interp, objPtr, argValObj, interp->framePtr->parent);
        Jim_DecrRefCount(interp, objPtr);
    }
    else {
        retcode = Jim_SetVariable(interp, argNameObj, argValObj);
    }
    return retcode;
}

static void JimSetProcWrongArgs(Jim_Interp *interp, Jim_Obj *procNameObj, Jim_Cmd *cmd)
{

    Jim_Obj *argmsg = Jim_NewStringObj(interp, "", 0);
    int i;

    for (i = 0; i < cmd->u.proc.argListLen; i++) {
        Jim_AppendString(interp, argmsg, " ", 1);

        if (i == cmd->u.proc.argsPos) {
            if (cmd->u.proc.arglist[i].defaultObjPtr) {

                Jim_AppendString(interp, argmsg, "?", 1);
                Jim_AppendObj(interp, argmsg, cmd->u.proc.arglist[i].defaultObjPtr);
                Jim_AppendString(interp, argmsg, " ...?", -1);
            }
            else {

                Jim_AppendString(interp, argmsg, "?arg...?", -1);
            }
        }
        else {
            if (cmd->u.proc.arglist[i].defaultObjPtr) {
                Jim_AppendString(interp, argmsg, "?", 1);
                Jim_AppendObj(interp, argmsg, cmd->u.proc.arglist[i].nameObjPtr);
                Jim_AppendString(interp, argmsg, "?", 1);
            }
            else {
                const char *arg = Jim_String(cmd->u.proc.arglist[i].nameObjPtr);
                if (*arg == '&') {
                    arg++;
                }
                Jim_AppendString(interp, argmsg, arg, -1);
            }
        }
    }
    Jim_SetResultFormatted(interp, "wrong # args: should be \"%#s%#s\"", procNameObj, argmsg);
}

#ifdef jim_ext_namespace
int Jim_EvalNamespace(Jim_Interp *interp, Jim_Obj *scriptObj, Jim_Obj *nsObj)
{
    Jim_CallFrame *callFramePtr;
    int retcode;


    callFramePtr = JimCreateCallFrame(interp, interp->framePtr, nsObj);
    callFramePtr->argv = &interp->emptyObj;
    callFramePtr->argc = 0;
    callFramePtr->procArgsObjPtr = NULL;
    callFramePtr->procBodyObjPtr = scriptObj;
    callFramePtr->staticVars = NULL;
    callFramePtr->fileNameObj = interp->emptyObj;
    callFramePtr->line = 0;
    Jim_IncrRefCount(scriptObj);
    interp->framePtr = callFramePtr;


    if (interp->framePtr->level == interp->maxCallFrameDepth) {
        Jim_SetResultString(interp, "Too many nested calls. Infinite recursion?", -1);
        retcode = JIM_ERR;
    }
    else {

        retcode = Jim_EvalObj(interp, scriptObj);
    }


    interp->framePtr = interp->framePtr->parent;
    JimFreeCallFrame(interp, callFramePtr, JIM_FCF_REUSE);

    return retcode;
}
#endif

static int JimCallProcedure(Jim_Interp *interp, Jim_Cmd *cmd, int argc, Jim_Obj *const *argv)
{
    Jim_CallFrame *callFramePtr;
    int i, d, retcode, optargs;
    ScriptObj *script;


    if (argc - 1 < cmd->u.proc.reqArity ||
        (cmd->u.proc.argsPos < 0 && argc - 1 > cmd->u.proc.reqArity + cmd->u.proc.optArity)) {
        JimSetProcWrongArgs(interp, argv[0], cmd);
        return JIM_ERR;
    }

    if (Jim_Length(cmd->u.proc.bodyObjPtr) == 0) {

        return JIM_OK;
    }


    if (interp->framePtr->level == interp->maxCallFrameDepth) {
        Jim_SetResultString(interp, "Too many nested calls. Infinite recursion?", -1);
        return JIM_ERR;
    }


    callFramePtr = JimCreateCallFrame(interp, interp->framePtr, cmd->u.proc.nsObj);
    callFramePtr->argv = argv;
    callFramePtr->argc = argc;
    callFramePtr->procArgsObjPtr = cmd->u.proc.argListObjPtr;
    callFramePtr->procBodyObjPtr = cmd->u.proc.bodyObjPtr;
    callFramePtr->staticVars = cmd->u.proc.staticVars;


    script = JimGetScript(interp, interp->currentScriptObj);
    callFramePtr->fileNameObj = script->fileNameObj;
    callFramePtr->line = script->linenr;

    Jim_IncrRefCount(cmd->u.proc.argListObjPtr);
    Jim_IncrRefCount(cmd->u.proc.bodyObjPtr);
    interp->framePtr = callFramePtr;


    optargs = (argc - 1 - cmd->u.proc.reqArity);


    i = 1;
    for (d = 0; d < cmd->u.proc.argListLen; d++) {
        Jim_Obj *nameObjPtr = cmd->u.proc.arglist[d].nameObjPtr;
        if (d == cmd->u.proc.argsPos) {

            Jim_Obj *listObjPtr;
            int argsLen = 0;
            if (cmd->u.proc.reqArity + cmd->u.proc.optArity < argc - 1) {
                argsLen = argc - 1 - (cmd->u.proc.reqArity + cmd->u.proc.optArity);
            }
            listObjPtr = Jim_NewListObj(interp, &argv[i], argsLen);


            if (cmd->u.proc.arglist[d].defaultObjPtr) {
                nameObjPtr =cmd->u.proc.arglist[d].defaultObjPtr;
            }
            retcode = Jim_SetVariable(interp, nameObjPtr, listObjPtr);
            if (retcode != JIM_OK) {
                goto badargset;
            }

            i += argsLen;
            continue;
        }


        if (cmd->u.proc.arglist[d].defaultObjPtr == NULL || optargs-- > 0) {
            retcode = JimSetProcArg(interp, nameObjPtr, argv[i++]);
        }
        else {

            retcode = Jim_SetVariable(interp, nameObjPtr, cmd->u.proc.arglist[d].defaultObjPtr);
        }
        if (retcode != JIM_OK) {
            goto badargset;
        }
    }


    retcode = Jim_EvalObj(interp, cmd->u.proc.bodyObjPtr);

badargset:


    retcode = JimInvokeDefer(interp, retcode);
    interp->framePtr = interp->framePtr->parent;
    JimFreeCallFrame(interp, callFramePtr, JIM_FCF_REUSE);


    if (interp->framePtr->tailcallObj) {
        do {
            Jim_Obj *tailcallObj = interp->framePtr->tailcallObj;

            interp->framePtr->tailcallObj = NULL;

            if (retcode == JIM_EVAL) {
                retcode = Jim_EvalObjList(interp, tailcallObj);
                if (retcode == JIM_RETURN) {
                    interp->returnLevel++;
                }
            }
            Jim_DecrRefCount(interp, tailcallObj);
        } while (interp->framePtr->tailcallObj);


        if (interp->framePtr->tailcallCmd) {
            JimDecrCmdRefCount(interp, interp->framePtr->tailcallCmd);
            interp->framePtr->tailcallCmd = NULL;
        }
    }


    if (retcode == JIM_RETURN) {
        if (--interp->returnLevel <= 0) {
            retcode = interp->returnCode;
            interp->returnCode = JIM_OK;
            interp->returnLevel = 0;
        }
    }
    else if (retcode == JIM_ERR) {
        interp->addStackTrace++;
        Jim_DecrRefCount(interp, interp->errorProc);
        interp->errorProc = argv[0];
        Jim_IncrRefCount(interp->errorProc);
    }

    return retcode;
}

int Jim_EvalSource(Jim_Interp *interp, const char *filename, int lineno, const char *script)
{
    int retval;
    Jim_Obj *scriptObjPtr;

    scriptObjPtr = Jim_NewStringObj(interp, script, -1);
    Jim_IncrRefCount(scriptObjPtr);

    if (filename) {
        Jim_Obj *prevScriptObj;

        JimSetSourceInfo(interp, scriptObjPtr, Jim_NewStringObj(interp, filename, -1), lineno);

        prevScriptObj = interp->currentScriptObj;
        interp->currentScriptObj = scriptObjPtr;

        retval = Jim_EvalObj(interp, scriptObjPtr);

        interp->currentScriptObj = prevScriptObj;
    }
    else {
        retval = Jim_EvalObj(interp, scriptObjPtr);
    }
    Jim_DecrRefCount(interp, scriptObjPtr);
    return retval;
}

int Jim_Eval(Jim_Interp *interp, const char *script)
{
    return Jim_EvalObj(interp, Jim_NewStringObj(interp, script, -1));
}


int Jim_EvalGlobal(Jim_Interp *interp, const char *script)
{
    int retval;
    Jim_CallFrame *savedFramePtr = interp->framePtr;

    interp->framePtr = interp->topFramePtr;
    retval = Jim_Eval(interp, script);
    interp->framePtr = savedFramePtr;

    return retval;
}

int Jim_EvalFileGlobal(Jim_Interp *interp, const char *filename)
{
    int retval;
    Jim_CallFrame *savedFramePtr = interp->framePtr;

    interp->framePtr = interp->topFramePtr;
    retval = Jim_EvalFile(interp, filename);
    interp->framePtr = savedFramePtr;

    return retval;
}

#include <sys/stat.h>

int Jim_EvalFile(Jim_Interp *interp, const char *filename)
{
    FILE *fp;
    char *buf;
    Jim_Obj *scriptObjPtr;
    Jim_Obj *prevScriptObj;
    struct stat sb;
    int retcode;
    int readlen;

    if (stat(filename, &sb) != 0 || (fp = fopen(filename, "rt")) == NULL) {
        Jim_SetResultFormatted(interp, "couldn't read file \"%s\": %s", filename, strerror(errno));
        return JIM_ERR;
    }
    if (sb.st_size == 0) {
        fclose(fp);
        return JIM_OK;
    }

    buf = Jim_Alloc(sb.st_size + 1);
    readlen = fread(buf, 1, sb.st_size, fp);
    if (ferror(fp)) {
        fclose(fp);
        Jim_Free(buf);
        Jim_SetResultFormatted(interp, "failed to load file \"%s\": %s", filename, strerror(errno));
        return JIM_ERR;
    }
    fclose(fp);
    buf[readlen] = 0;

    scriptObjPtr = Jim_NewStringObjNoAlloc(interp, buf, readlen);
    JimSetSourceInfo(interp, scriptObjPtr, Jim_NewStringObj(interp, filename, -1), 1);
    Jim_IncrRefCount(scriptObjPtr);

    prevScriptObj = interp->currentScriptObj;
    interp->currentScriptObj = scriptObjPtr;

    retcode = Jim_EvalObj(interp, scriptObjPtr);


    if (retcode == JIM_RETURN) {
        if (--interp->returnLevel <= 0) {
            retcode = interp->returnCode;
            interp->returnCode = JIM_OK;
            interp->returnLevel = 0;
        }
    }
    if (retcode == JIM_ERR) {

        interp->addStackTrace++;
    }

    interp->currentScriptObj = prevScriptObj;

    Jim_DecrRefCount(interp, scriptObjPtr);

    return retcode;
}

static void JimParseSubst(struct JimParserCtx *pc, int flags)
{
    pc->tstart = pc->p;
    pc->tline = pc->linenr;

    if (pc->len == 0) {
        pc->tend = pc->p;
        pc->tt = JIM_TT_EOL;
        pc->eof = 1;
        return;
    }
    if (*pc->p == '[' && !(flags & JIM_SUBST_NOCMD)) {
        JimParseCmd(pc);
        return;
    }
    if (*pc->p == '$' && !(flags & JIM_SUBST_NOVAR)) {
        if (JimParseVar(pc) == JIM_OK) {
            return;
        }

        pc->tstart = pc->p;
        flags |= JIM_SUBST_NOVAR;
    }
    while (pc->len) {
        if (*pc->p == '$' && !(flags & JIM_SUBST_NOVAR)) {
            break;
        }
        if (*pc->p == '[' && !(flags & JIM_SUBST_NOCMD)) {
            break;
        }
        if (*pc->p == '\\' && pc->len > 1) {
            pc->p++;
            pc->len--;
        }
        pc->p++;
        pc->len--;
    }
    pc->tend = pc->p - 1;
    pc->tt = (flags & JIM_SUBST_NOESC) ? JIM_TT_STR : JIM_TT_ESC;
}


static int SetSubstFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr, int flags)
{
    int scriptTextLen;
    const char *scriptText = Jim_GetString(objPtr, &scriptTextLen);
    struct JimParserCtx parser;
    struct ScriptObj *script = Jim_Alloc(sizeof(*script));
    ParseTokenList tokenlist;


    ScriptTokenListInit(&tokenlist);

    JimParserInit(&parser, scriptText, scriptTextLen, 1);
    while (1) {
        JimParseSubst(&parser, flags);
        if (parser.eof) {

            break;
        }
        ScriptAddToken(&tokenlist, parser.tstart, parser.tend - parser.tstart + 1, parser.tt,
            parser.tline);
    }


    script->inUse = 1;
    script->substFlags = flags;
    script->fileNameObj = interp->emptyObj;
    Jim_IncrRefCount(script->fileNameObj);
    SubstObjAddTokens(interp, script, &tokenlist);


    ScriptTokenListFree(&tokenlist);

#ifdef DEBUG_SHOW_SUBST
    {
        int i;

        printf("==== Subst ====\n");
        for (i = 0; i < script->len; i++) {
            printf("[%2d] %s '%s'\n", i, jim_tt_name(script->token[i].type),
                Jim_String(script->token[i].objPtr));
        }
    }
#endif


    Jim_FreeIntRep(interp, objPtr);
    Jim_SetIntRepPtr(objPtr, script);
    objPtr->typePtr = &scriptObjType;
    return JIM_OK;
}

static ScriptObj *Jim_GetSubst(Jim_Interp *interp, Jim_Obj *objPtr, int flags)
{
    if (objPtr->typePtr != &scriptObjType || ((ScriptObj *)Jim_GetIntRepPtr(objPtr))->substFlags != flags)
        SetSubstFromAny(interp, objPtr, flags);
    return (ScriptObj *) Jim_GetIntRepPtr(objPtr);
}

int Jim_SubstObj(Jim_Interp *interp, Jim_Obj *substObjPtr, Jim_Obj **resObjPtrPtr, int flags)
{
    ScriptObj *script = Jim_GetSubst(interp, substObjPtr, flags);

    Jim_IncrRefCount(substObjPtr);
    script->inUse++;

    *resObjPtrPtr = JimInterpolateTokens(interp, script->token, script->len, flags);

    script->inUse--;
    Jim_DecrRefCount(interp, substObjPtr);
    if (*resObjPtrPtr == NULL) {
        return JIM_ERR;
    }
    return JIM_OK;
}

void Jim_WrongNumArgs(Jim_Interp *interp, int argc, Jim_Obj *const *argv, const char *msg)
{
    Jim_Obj *objPtr;
    Jim_Obj *listObjPtr;

    JimPanic((argc == 0, "Jim_WrongNumArgs() called with argc=0"));

    listObjPtr = Jim_NewListObj(interp, argv, argc);

    if (msg && *msg) {
        Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, msg, -1));
    }
    Jim_IncrRefCount(listObjPtr);
    objPtr = Jim_ListJoin(interp, listObjPtr, " ", 1);
    Jim_DecrRefCount(interp, listObjPtr);

    Jim_SetResultFormatted(interp, "wrong # args: should be \"%#s\"", objPtr);
}

typedef void JimHashtableIteratorCallbackType(Jim_Interp *interp, Jim_Obj *listObjPtr,
    Jim_HashEntry *he, int type);

#define JimTrivialMatch(pattern)    (strpbrk((pattern), "*[?\\") == NULL)

static Jim_Obj *JimHashtablePatternMatch(Jim_Interp *interp, Jim_HashTable *ht, Jim_Obj *patternObjPtr,
    JimHashtableIteratorCallbackType *callback, int type)
{
    Jim_HashEntry *he;
    Jim_Obj *listObjPtr = Jim_NewListObj(interp, NULL, 0);


    if (patternObjPtr && JimTrivialMatch(Jim_String(patternObjPtr))) {
        he = Jim_FindHashEntry(ht, Jim_String(patternObjPtr));
        if (he) {
            callback(interp, listObjPtr, he, type);
        }
    }
    else {
        Jim_HashTableIterator htiter;
        JimInitHashTableIterator(ht, &htiter);
        while ((he = Jim_NextHashEntry(&htiter)) != NULL) {
            if (patternObjPtr == NULL || JimGlobMatch(Jim_String(patternObjPtr), he->key, 0)) {
                callback(interp, listObjPtr, he, type);
            }
        }
    }
    return listObjPtr;
}


#define JIM_CMDLIST_COMMANDS 0
#define JIM_CMDLIST_PROCS 1
#define JIM_CMDLIST_CHANNELS 2

static void JimCommandMatch(Jim_Interp *interp, Jim_Obj *listObjPtr,
    Jim_HashEntry *he, int type)
{
    Jim_Cmd *cmdPtr = Jim_GetHashEntryVal(he);
    Jim_Obj *objPtr;

    if (type == JIM_CMDLIST_PROCS && !cmdPtr->isproc) {

        return;
    }

    objPtr = Jim_NewStringObj(interp, he->key, -1);
    Jim_IncrRefCount(objPtr);

    if (type != JIM_CMDLIST_CHANNELS || Jim_AioFilehandle(interp, objPtr)) {
        Jim_ListAppendElement(interp, listObjPtr, objPtr);
    }
    Jim_DecrRefCount(interp, objPtr);
}


static Jim_Obj *JimCommandsList(Jim_Interp *interp, Jim_Obj *patternObjPtr, int type)
{
    return JimHashtablePatternMatch(interp, &interp->commands, patternObjPtr, JimCommandMatch, type);
}


#define JIM_VARLIST_GLOBALS 0
#define JIM_VARLIST_LOCALS 1
#define JIM_VARLIST_VARS 2

#define JIM_VARLIST_VALUES 0x1000

static void JimVariablesMatch(Jim_Interp *interp, Jim_Obj *listObjPtr,
    Jim_HashEntry *he, int type)
{
    Jim_Var *varPtr = Jim_GetHashEntryVal(he);

    if (type != JIM_VARLIST_LOCALS || varPtr->linkFramePtr == NULL) {
        Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, he->key, -1));
        if (type & JIM_VARLIST_VALUES) {
            Jim_ListAppendElement(interp, listObjPtr, varPtr->objPtr);
        }
    }
}


static Jim_Obj *JimVariablesList(Jim_Interp *interp, Jim_Obj *patternObjPtr, int mode)
{
    if (mode == JIM_VARLIST_LOCALS && interp->framePtr == interp->topFramePtr) {
        return interp->emptyObj;
    }
    else {
        Jim_CallFrame *framePtr = (mode == JIM_VARLIST_GLOBALS) ? interp->topFramePtr : interp->framePtr;
        return JimHashtablePatternMatch(interp, &framePtr->vars, patternObjPtr, JimVariablesMatch, mode);
    }
}

static int JimInfoLevel(Jim_Interp *interp, Jim_Obj *levelObjPtr,
    Jim_Obj **objPtrPtr, int info_level_cmd)
{
    Jim_CallFrame *targetCallFrame;

    targetCallFrame = JimGetCallFrameByInteger(interp, levelObjPtr);
    if (targetCallFrame == NULL) {
        return JIM_ERR;
    }

    if (targetCallFrame == interp->topFramePtr) {
        Jim_SetResultFormatted(interp, "bad level \"%#s\"", levelObjPtr);
        return JIM_ERR;
    }
    if (info_level_cmd) {
        *objPtrPtr = Jim_NewListObj(interp, targetCallFrame->argv, targetCallFrame->argc);
    }
    else {
        Jim_Obj *listObj = Jim_NewListObj(interp, NULL, 0);

        Jim_ListAppendElement(interp, listObj, targetCallFrame->argv[0]);
        Jim_ListAppendElement(interp, listObj, targetCallFrame->fileNameObj);
        Jim_ListAppendElement(interp, listObj, Jim_NewIntObj(interp, targetCallFrame->line));
        *objPtrPtr = listObj;
    }
    return JIM_OK;
}



static int Jim_PutsCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "?-nonewline? string");
        return JIM_ERR;
    }
    if (argc == 3) {
        if (!Jim_CompareStringImmediate(interp, argv[1], "-nonewline")) {
            Jim_SetResultString(interp, "The second argument must " "be -nonewline", -1);
            return JIM_ERR;
        }
        else {
            fputs(Jim_String(argv[2]), stdout);
        }
    }
    else {
        puts(Jim_String(argv[1]));
    }
    return JIM_OK;
}


static int JimAddMulHelper(Jim_Interp *interp, int argc, Jim_Obj *const *argv, int op)
{
    jim_wide wideValue, res;
    double doubleValue, doubleRes;
    int i;

    res = (op == JIM_EXPROP_ADD) ? 0 : 1;

    for (i = 1; i < argc; i++) {
        if (Jim_GetWide(interp, argv[i], &wideValue) != JIM_OK)
            goto trydouble;
        if (op == JIM_EXPROP_ADD)
            res += wideValue;
        else
            res *= wideValue;
    }
    Jim_SetResultInt(interp, res);
    return JIM_OK;
  trydouble:
    doubleRes = (double)res;
    for (; i < argc; i++) {
        if (Jim_GetDouble(interp, argv[i], &doubleValue) != JIM_OK)
            return JIM_ERR;
        if (op == JIM_EXPROP_ADD)
            doubleRes += doubleValue;
        else
            doubleRes *= doubleValue;
    }
    Jim_SetResult(interp, Jim_NewDoubleObj(interp, doubleRes));
    return JIM_OK;
}


static int JimSubDivHelper(Jim_Interp *interp, int argc, Jim_Obj *const *argv, int op)
{
    jim_wide wideValue, res = 0;
    double doubleValue, doubleRes = 0;
    int i = 2;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "number ?number ... number?");
        return JIM_ERR;
    }
    else if (argc == 2) {
        if (Jim_GetWide(interp, argv[1], &wideValue) != JIM_OK) {
            if (Jim_GetDouble(interp, argv[1], &doubleValue) != JIM_OK) {
                return JIM_ERR;
            }
            else {
                if (op == JIM_EXPROP_SUB)
                    doubleRes = -doubleValue;
                else
                    doubleRes = 1.0 / doubleValue;
                Jim_SetResult(interp, Jim_NewDoubleObj(interp, doubleRes));
                return JIM_OK;
            }
        }
        if (op == JIM_EXPROP_SUB) {
            res = -wideValue;
            Jim_SetResultInt(interp, res);
        }
        else {
            doubleRes = 1.0 / wideValue;
            Jim_SetResult(interp, Jim_NewDoubleObj(interp, doubleRes));
        }
        return JIM_OK;
    }
    else {
        if (Jim_GetWide(interp, argv[1], &res) != JIM_OK) {
            if (Jim_GetDouble(interp, argv[1], &doubleRes)
                != JIM_OK) {
                return JIM_ERR;
            }
            else {
                goto trydouble;
            }
        }
    }
    for (i = 2; i < argc; i++) {
        if (Jim_GetWide(interp, argv[i], &wideValue) != JIM_OK) {
            doubleRes = (double)res;
            goto trydouble;
        }
        if (op == JIM_EXPROP_SUB)
            res -= wideValue;
        else {
            if (wideValue == 0) {
                Jim_SetResultString(interp, "Division by zero", -1);
                return JIM_ERR;
            }
            res /= wideValue;
        }
    }
    Jim_SetResultInt(interp, res);
    return JIM_OK;
  trydouble:
    for (; i < argc; i++) {
        if (Jim_GetDouble(interp, argv[i], &doubleValue) != JIM_OK)
            return JIM_ERR;
        if (op == JIM_EXPROP_SUB)
            doubleRes -= doubleValue;
        else
            doubleRes /= doubleValue;
    }
    Jim_SetResult(interp, Jim_NewDoubleObj(interp, doubleRes));
    return JIM_OK;
}



static int Jim_AddCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimAddMulHelper(interp, argc, argv, JIM_EXPROP_ADD);
}


static int Jim_MulCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimAddMulHelper(interp, argc, argv, JIM_EXPROP_MUL);
}


static int Jim_SubCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimSubDivHelper(interp, argc, argv, JIM_EXPROP_SUB);
}


static int Jim_DivCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimSubDivHelper(interp, argc, argv, JIM_EXPROP_DIV);
}


static int Jim_SetCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "varName ?newValue?");
        return JIM_ERR;
    }
    if (argc == 2) {
        Jim_Obj *objPtr;

        objPtr = Jim_GetVariable(interp, argv[1], JIM_ERRMSG);
        if (!objPtr)
            return JIM_ERR;
        Jim_SetResult(interp, objPtr);
        return JIM_OK;
    }

    if (Jim_SetVariable(interp, argv[1], argv[2]) != JIM_OK)
        return JIM_ERR;
    Jim_SetResult(interp, argv[2]);
    return JIM_OK;
}

static int Jim_UnsetCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i = 1;
    int complain = 1;

    while (i < argc) {
        if (Jim_CompareStringImmediate(interp, argv[i], "--")) {
            i++;
            break;
        }
        if (Jim_CompareStringImmediate(interp, argv[i], "-nocomplain")) {
            complain = 0;
            i++;
            continue;
        }
        break;
    }

    while (i < argc) {
        if (Jim_UnsetVariable(interp, argv[i], complain ? JIM_ERRMSG : JIM_NONE) != JIM_OK
            && complain) {
            return JIM_ERR;
        }
        i++;
    }
    return JIM_OK;
}


static int Jim_WhileCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "condition body");
        return JIM_ERR;
    }


    while (1) {
        int boolean, retval;

        if ((retval = Jim_GetBoolFromExpr(interp, argv[1], &boolean)) != JIM_OK)
            return retval;
        if (!boolean)
            break;

        if ((retval = Jim_EvalObj(interp, argv[2])) != JIM_OK) {
            switch (retval) {
                case JIM_BREAK:
                    goto out;
                    break;
                case JIM_CONTINUE:
                    continue;
                    break;
                default:
                    return retval;
            }
        }
    }
  out:
    Jim_SetEmptyResult(interp);
    return JIM_OK;
}


static int Jim_ForCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int retval;
    int boolean = 1;
    Jim_Obj *varNamePtr = NULL;
    Jim_Obj *stopVarNamePtr = NULL;

    if (argc != 5) {
        Jim_WrongNumArgs(interp, 1, argv, "start test next body");
        return JIM_ERR;
    }


    if ((retval = Jim_EvalObj(interp, argv[1])) != JIM_OK) {
        return retval;
    }

    retval = Jim_GetBoolFromExpr(interp, argv[2], &boolean);


#ifdef JIM_OPTIMIZATION
    if (retval == JIM_OK && boolean) {
        ScriptObj *incrScript;
        struct ExprTree *expr;
        jim_wide stop, currentVal;
        Jim_Obj *objPtr;
        int cmpOffset;


        expr = JimGetExpression(interp, argv[2]);
        incrScript = JimGetScript(interp, argv[3]);


        if (incrScript == NULL || incrScript->len != 3 || !expr || expr->len != 3) {
            goto evalstart;
        }

        if (incrScript->token[1].type != JIM_TT_ESC) {
            goto evalstart;
        }

        if (expr->expr->type == JIM_EXPROP_LT) {
            cmpOffset = 0;
        }
        else if (expr->expr->type == JIM_EXPROP_LTE) {
            cmpOffset = 1;
        }
        else {
            goto evalstart;
        }

        if (expr->expr->left->type != JIM_TT_VAR) {
            goto evalstart;
        }

        if (expr->expr->right->type != JIM_TT_VAR && expr->expr->right->type != JIM_TT_EXPR_INT) {
            goto evalstart;
        }


        if (!Jim_CompareStringImmediate(interp, incrScript->token[1].objPtr, "incr")) {
            goto evalstart;
        }


        if (!Jim_StringEqObj(incrScript->token[2].objPtr, expr->expr->left->objPtr)) {
            goto evalstart;
        }


        if (expr->expr->right->type == JIM_TT_EXPR_INT) {
            if (Jim_GetWide(interp, expr->expr->right->objPtr, &stop) == JIM_ERR) {
                goto evalstart;
            }
        }
        else {
            stopVarNamePtr = expr->expr->right->objPtr;
            Jim_IncrRefCount(stopVarNamePtr);

            stop = 0;
        }


        varNamePtr = expr->expr->left->objPtr;
        Jim_IncrRefCount(varNamePtr);

        objPtr = Jim_GetVariable(interp, varNamePtr, JIM_NONE);
        if (objPtr == NULL || Jim_GetWide(interp, objPtr, &currentVal) != JIM_OK) {
            goto testcond;
        }


        while (retval == JIM_OK) {




            if (stopVarNamePtr) {
                objPtr = Jim_GetVariable(interp, stopVarNamePtr, JIM_NONE);
                if (objPtr == NULL || Jim_GetWide(interp, objPtr, &stop) != JIM_OK) {
                    goto testcond;
                }
            }

            if (currentVal >= stop + cmpOffset) {
                break;
            }


            retval = Jim_EvalObj(interp, argv[4]);
            if (retval == JIM_OK || retval == JIM_CONTINUE) {
                retval = JIM_OK;

                objPtr = Jim_GetVariable(interp, varNamePtr, JIM_ERRMSG);


                if (objPtr == NULL) {
                    retval = JIM_ERR;
                    goto out;
                }
                if (!Jim_IsShared(objPtr) && objPtr->typePtr == &intObjType) {
                    currentVal = ++JimWideValue(objPtr);
                    Jim_InvalidateStringRep(objPtr);
                }
                else {
                    if (Jim_GetWide(interp, objPtr, &currentVal) != JIM_OK ||
                        Jim_SetVariable(interp, varNamePtr, Jim_NewIntObj(interp,
                                ++currentVal)) != JIM_OK) {
                        goto evalnext;
                    }
                }
            }
        }
        goto out;
    }
  evalstart:
#endif

    while (boolean && (retval == JIM_OK || retval == JIM_CONTINUE)) {

        retval = Jim_EvalObj(interp, argv[4]);

        if (retval == JIM_OK || retval == JIM_CONTINUE) {

JIM_IF_OPTIM(evalnext:)
            retval = Jim_EvalObj(interp, argv[3]);
            if (retval == JIM_OK || retval == JIM_CONTINUE) {

JIM_IF_OPTIM(testcond:)
                retval = Jim_GetBoolFromExpr(interp, argv[2], &boolean);
            }
        }
    }
JIM_IF_OPTIM(out:)
    if (stopVarNamePtr) {
        Jim_DecrRefCount(interp, stopVarNamePtr);
    }
    if (varNamePtr) {
        Jim_DecrRefCount(interp, varNamePtr);
    }

    if (retval == JIM_CONTINUE || retval == JIM_BREAK || retval == JIM_OK) {
        Jim_SetEmptyResult(interp);
        return JIM_OK;
    }

    return retval;
}


static int Jim_LoopCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int retval;
    jim_wide i;
    jim_wide limit;
    jim_wide incr = 1;
    Jim_Obj *bodyObjPtr;

    if (argc != 5 && argc != 6) {
        Jim_WrongNumArgs(interp, 1, argv, "var first limit ?incr? body");
        return JIM_ERR;
    }

    if (Jim_GetWide(interp, argv[2], &i) != JIM_OK ||
        Jim_GetWide(interp, argv[3], &limit) != JIM_OK ||
          (argc == 6 && Jim_GetWide(interp, argv[4], &incr) != JIM_OK)) {
        return JIM_ERR;
    }
    bodyObjPtr = (argc == 5) ? argv[4] : argv[5];

    retval = Jim_SetVariable(interp, argv[1], argv[2]);

    while (((i < limit && incr > 0) || (i > limit && incr < 0)) && retval == JIM_OK) {
        retval = Jim_EvalObj(interp, bodyObjPtr);
        if (retval == JIM_OK || retval == JIM_CONTINUE) {
            Jim_Obj *objPtr = Jim_GetVariable(interp, argv[1], JIM_ERRMSG);

            retval = JIM_OK;


            i += incr;

            if (objPtr && !Jim_IsShared(objPtr) && objPtr->typePtr == &intObjType) {
                if (argv[1]->typePtr != &variableObjType) {
                    if (Jim_SetVariable(interp, argv[1], objPtr) != JIM_OK) {
                        return JIM_ERR;
                    }
                }
                JimWideValue(objPtr) = i;
                Jim_InvalidateStringRep(objPtr);

                if (argv[1]->typePtr != &variableObjType) {
                    if (Jim_SetVariable(interp, argv[1], objPtr) != JIM_OK) {
                        retval = JIM_ERR;
                        break;
                    }
                }
            }
            else {
                objPtr = Jim_NewIntObj(interp, i);
                retval = Jim_SetVariable(interp, argv[1], objPtr);
                if (retval != JIM_OK) {
                    Jim_FreeNewObj(interp, objPtr);
                }
            }
        }
    }

    if (retval == JIM_OK || retval == JIM_CONTINUE || retval == JIM_BREAK) {
        Jim_SetEmptyResult(interp);
        return JIM_OK;
    }
    return retval;
}

typedef struct {
    Jim_Obj *objPtr;
    int idx;
} Jim_ListIter;

static void JimListIterInit(Jim_ListIter *iter, Jim_Obj *objPtr)
{
    iter->objPtr = objPtr;
    iter->idx = 0;
}

static Jim_Obj *JimListIterNext(Jim_Interp *interp, Jim_ListIter *iter)
{
    if (iter->idx >= Jim_ListLength(interp, iter->objPtr)) {
        return NULL;
    }
    return iter->objPtr->internalRep.listValue.ele[iter->idx++];
}

static int JimListIterDone(Jim_Interp *interp, Jim_ListIter *iter)
{
    return iter->idx >= Jim_ListLength(interp, iter->objPtr);
}


static int JimForeachMapHelper(Jim_Interp *interp, int argc, Jim_Obj *const *argv, int doMap)
{
    int result = JIM_OK;
    int i, numargs;
    Jim_ListIter twoiters[2];
    Jim_ListIter *iters;
    Jim_Obj *script;
    Jim_Obj *resultObj;

    if (argc < 4 || argc % 2 != 0) {
        Jim_WrongNumArgs(interp, 1, argv, "varList list ?varList list ...? script");
        return JIM_ERR;
    }
    script = argv[argc - 1];
    numargs = (argc - 1 - 1);

    if (numargs == 2) {
        iters = twoiters;
    }
    else {
        iters = Jim_Alloc(numargs * sizeof(*iters));
    }
    for (i = 0; i < numargs; i++) {
        JimListIterInit(&iters[i], argv[i + 1]);
        if (i % 2 == 0 && JimListIterDone(interp, &iters[i])) {
            result = JIM_ERR;
        }
    }
    if (result != JIM_OK) {
        Jim_SetResultString(interp, "foreach varlist is empty", -1);
        goto empty_varlist;
    }

    if (doMap) {
        resultObj = Jim_NewListObj(interp, NULL, 0);
    }
    else {
        resultObj = interp->emptyObj;
    }
    Jim_IncrRefCount(resultObj);

    while (1) {

        for (i = 0; i < numargs; i += 2) {
            if (!JimListIterDone(interp, &iters[i + 1])) {
                break;
            }
        }
        if (i == numargs) {

            break;
        }


        for (i = 0; i < numargs; i += 2) {
            Jim_Obj *varName;


            JimListIterInit(&iters[i], argv[i + 1]);
            while ((varName = JimListIterNext(interp, &iters[i])) != NULL) {
                Jim_Obj *valObj = JimListIterNext(interp, &iters[i + 1]);
                if (!valObj) {

                    valObj = interp->emptyObj;
                }

                Jim_IncrRefCount(valObj);
                result = Jim_SetVariable(interp, varName, valObj);
                Jim_DecrRefCount(interp, valObj);
                if (result != JIM_OK) {
                    goto err;
                }
            }
        }
        switch (result = Jim_EvalObj(interp, script)) {
            case JIM_OK:
                if (doMap) {
                    Jim_ListAppendElement(interp, resultObj, interp->result);
                }
                break;
            case JIM_CONTINUE:
                break;
            case JIM_BREAK:
                goto out;
            default:
                goto err;
        }
    }
  out:
    result = JIM_OK;
    Jim_SetResult(interp, resultObj);
  err:
    Jim_DecrRefCount(interp, resultObj);
  empty_varlist:
    if (numargs > 2) {
        Jim_Free(iters);
    }
    return result;
}


static int Jim_ForeachCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimForeachMapHelper(interp, argc, argv, 0);
}


static int Jim_LmapCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimForeachMapHelper(interp, argc, argv, 1);
}


static int Jim_LassignCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int result = JIM_ERR;
    int i;
    Jim_ListIter iter;
    Jim_Obj *resultObj;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "varList list ?varName ...?");
        return JIM_ERR;
    }

    JimListIterInit(&iter, argv[1]);

    for (i = 2; i < argc; i++) {
        Jim_Obj *valObj = JimListIterNext(interp, &iter);
        result = Jim_SetVariable(interp, argv[i], valObj ? valObj : interp->emptyObj);
        if (result != JIM_OK) {
            return result;
        }
    }

    resultObj = Jim_NewListObj(interp, NULL, 0);
    while (!JimListIterDone(interp, &iter)) {
        Jim_ListAppendElement(interp, resultObj, JimListIterNext(interp, &iter));
    }

    Jim_SetResult(interp, resultObj);

    return JIM_OK;
}


static int Jim_IfCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int boolean, retval, current = 1, falsebody = 0;

    if (argc >= 3) {
        while (1) {

            if (current >= argc)
                goto err;
            if ((retval = Jim_GetBoolFromExpr(interp, argv[current++], &boolean))
                != JIM_OK)
                return retval;

            if (current >= argc)
                goto err;
            if (Jim_CompareStringImmediate(interp, argv[current], "then"))
                current++;

            if (current >= argc)
                goto err;
            if (boolean)
                return Jim_EvalObj(interp, argv[current]);

            if (++current >= argc) {
                Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
                return JIM_OK;
            }
            falsebody = current++;
            if (Jim_CompareStringImmediate(interp, argv[falsebody], "else")) {

                if (current != argc - 1)
                    goto err;
                return Jim_EvalObj(interp, argv[current]);
            }
            else if (Jim_CompareStringImmediate(interp, argv[falsebody], "elseif"))
                continue;

            else if (falsebody != argc - 1)
                goto err;
            return Jim_EvalObj(interp, argv[falsebody]);
        }
        return JIM_OK;
    }
  err:
    Jim_WrongNumArgs(interp, 1, argv, "condition ?then? trueBody ?elseif ...? ?else? falseBody");
    return JIM_ERR;
}



int Jim_CommandMatchObj(Jim_Interp *interp, Jim_Obj *commandObj, Jim_Obj *patternObj,
    Jim_Obj *stringObj, int nocase)
{
    Jim_Obj *parms[4];
    int argc = 0;
    long eq;
    int rc;

    parms[argc++] = commandObj;
    if (nocase) {
        parms[argc++] = Jim_NewStringObj(interp, "-nocase", -1);
    }
    parms[argc++] = patternObj;
    parms[argc++] = stringObj;

    rc = Jim_EvalObjVector(interp, argc, parms);

    if (rc != JIM_OK || Jim_GetLong(interp, Jim_GetResult(interp), &eq) != JIM_OK) {
        eq = -rc;
    }

    return eq;
}


static int Jim_SwitchCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    enum { SWITCH_EXACT, SWITCH_GLOB, SWITCH_RE, SWITCH_CMD };
    int matchOpt = SWITCH_EXACT, opt = 1, patCount, i;
    Jim_Obj *command = NULL, *scriptObj = NULL, *strObj;
    Jim_Obj **caseList;

    if (argc < 3) {
      wrongnumargs:
        Jim_WrongNumArgs(interp, 1, argv, "?options? string "
            "pattern body ... ?default body?   or   " "{pattern body ?pattern body ...?}");
        return JIM_ERR;
    }
    for (opt = 1; opt < argc; ++opt) {
        const char *option = Jim_String(argv[opt]);

        if (*option != '-')
            break;
        else if (strncmp(option, "--", 2) == 0) {
            ++opt;
            break;
        }
        else if (strncmp(option, "-exact", 2) == 0)
            matchOpt = SWITCH_EXACT;
        else if (strncmp(option, "-glob", 2) == 0)
            matchOpt = SWITCH_GLOB;
        else if (strncmp(option, "-regexp", 2) == 0)
            matchOpt = SWITCH_RE;
        else if (strncmp(option, "-command", 2) == 0) {
            matchOpt = SWITCH_CMD;
            if ((argc - opt) < 2)
                goto wrongnumargs;
            command = argv[++opt];
        }
        else {
            Jim_SetResultFormatted(interp,
                "bad option \"%#s\": must be -exact, -glob, -regexp, -command procname or --",
                argv[opt]);
            return JIM_ERR;
        }
        if ((argc - opt) < 2)
            goto wrongnumargs;
    }
    strObj = argv[opt++];
    patCount = argc - opt;
    if (patCount == 1) {
        JimListGetElements(interp, argv[opt], &patCount, &caseList);
    }
    else
        caseList = (Jim_Obj **)&argv[opt];
    if (patCount == 0 || patCount % 2 != 0)
        goto wrongnumargs;
    for (i = 0; scriptObj == NULL && i < patCount; i += 2) {
        Jim_Obj *patObj = caseList[i];

        if (!Jim_CompareStringImmediate(interp, patObj, "default")
            || i < (patCount - 2)) {
            switch (matchOpt) {
                case SWITCH_EXACT:
                    if (Jim_StringEqObj(strObj, patObj))
                        scriptObj = caseList[i + 1];
                    break;
                case SWITCH_GLOB:
                    if (Jim_StringMatchObj(interp, patObj, strObj, 0))
                        scriptObj = caseList[i + 1];
                    break;
                case SWITCH_RE:
                    command = Jim_NewStringObj(interp, "regexp", -1);

                case SWITCH_CMD:{
                        int rc = Jim_CommandMatchObj(interp, command, patObj, strObj, 0);

                        if (argc - opt == 1) {
                            JimListGetElements(interp, argv[opt], &patCount, &caseList);
                        }

                        if (rc < 0) {
                            return -rc;
                        }
                        if (rc)
                            scriptObj = caseList[i + 1];
                        break;
                    }
            }
        }
        else {
            scriptObj = caseList[i + 1];
        }
    }
    for (; i < patCount && Jim_CompareStringImmediate(interp, scriptObj, "-"); i += 2)
        scriptObj = caseList[i + 1];
    if (scriptObj && Jim_CompareStringImmediate(interp, scriptObj, "-")) {
        Jim_SetResultFormatted(interp, "no body specified for pattern \"%#s\"", caseList[i - 2]);
        return JIM_ERR;
    }
    Jim_SetEmptyResult(interp);
    if (scriptObj) {
        return Jim_EvalObj(interp, scriptObj);
    }
    return JIM_OK;
}


static int Jim_ListCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *listObjPtr;

    listObjPtr = Jim_NewListObj(interp, argv + 1, argc - 1);
    Jim_SetResult(interp, listObjPtr);
    return JIM_OK;
}


static int Jim_LindexCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr, *listObjPtr;
    int i;
    int idx;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "list ?index ...?");
        return JIM_ERR;
    }
    objPtr = argv[1];
    Jim_IncrRefCount(objPtr);
    for (i = 2; i < argc; i++) {
        listObjPtr = objPtr;
        if (Jim_GetIndex(interp, argv[i], &idx) != JIM_OK) {
            Jim_DecrRefCount(interp, listObjPtr);
            return JIM_ERR;
        }
        if (Jim_ListIndex(interp, listObjPtr, idx, &objPtr, JIM_NONE) != JIM_OK) {
            Jim_DecrRefCount(interp, listObjPtr);
            Jim_SetEmptyResult(interp);
            return JIM_OK;
        }
        Jim_IncrRefCount(objPtr);
        Jim_DecrRefCount(interp, listObjPtr);
    }
    Jim_SetResult(interp, objPtr);
    Jim_DecrRefCount(interp, objPtr);
    return JIM_OK;
}


static int Jim_LlengthCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "list");
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, Jim_ListLength(interp, argv[1]));
    return JIM_OK;
}


static int Jim_LsearchCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    static const char * const options[] = {
        "-bool", "-not", "-nocase", "-exact", "-glob", "-regexp", "-all", "-inline", "-command",
            NULL
    };
    enum
    { OPT_BOOL, OPT_NOT, OPT_NOCASE, OPT_EXACT, OPT_GLOB, OPT_REGEXP, OPT_ALL, OPT_INLINE,
            OPT_COMMAND };
    int i;
    int opt_bool = 0;
    int opt_not = 0;
    int opt_nocase = 0;
    int opt_all = 0;
    int opt_inline = 0;
    int opt_match = OPT_EXACT;
    int listlen;
    int rc = JIM_OK;
    Jim_Obj *listObjPtr = NULL;
    Jim_Obj *commandObj = NULL;

    if (argc < 3) {
      wrongargs:
        Jim_WrongNumArgs(interp, 1, argv,
            "?-exact|-glob|-regexp|-command 'command'? ?-bool|-inline? ?-not? ?-nocase? ?-all? list value");
        return JIM_ERR;
    }

    for (i = 1; i < argc - 2; i++) {
        int option;

        if (Jim_GetEnum(interp, argv[i], options, &option, NULL, JIM_ERRMSG) != JIM_OK) {
            return JIM_ERR;
        }
        switch (option) {
            case OPT_BOOL:
                opt_bool = 1;
                opt_inline = 0;
                break;
            case OPT_NOT:
                opt_not = 1;
                break;
            case OPT_NOCASE:
                opt_nocase = 1;
                break;
            case OPT_INLINE:
                opt_inline = 1;
                opt_bool = 0;
                break;
            case OPT_ALL:
                opt_all = 1;
                break;
            case OPT_COMMAND:
                if (i >= argc - 2) {
                    goto wrongargs;
                }
                commandObj = argv[++i];

            case OPT_EXACT:
            case OPT_GLOB:
            case OPT_REGEXP:
                opt_match = option;
                break;
        }
    }

    argv += i;

    if (opt_all) {
        listObjPtr = Jim_NewListObj(interp, NULL, 0);
    }
    if (opt_match == OPT_REGEXP) {
        commandObj = Jim_NewStringObj(interp, "regexp", -1);
    }
    if (commandObj) {
        Jim_IncrRefCount(commandObj);
    }

    listlen = Jim_ListLength(interp, argv[0]);
    for (i = 0; i < listlen; i++) {
        int eq = 0;
        Jim_Obj *objPtr = Jim_ListGetIndex(interp, argv[0], i);

        switch (opt_match) {
            case OPT_EXACT:
                eq = Jim_StringCompareObj(interp, argv[1], objPtr, opt_nocase) == 0;
                break;

            case OPT_GLOB:
                eq = Jim_StringMatchObj(interp, argv[1], objPtr, opt_nocase);
                break;

            case OPT_REGEXP:
            case OPT_COMMAND:
                eq = Jim_CommandMatchObj(interp, commandObj, argv[1], objPtr, opt_nocase);
                if (eq < 0) {
                    if (listObjPtr) {
                        Jim_FreeNewObj(interp, listObjPtr);
                    }
                    rc = JIM_ERR;
                    goto done;
                }
                break;
        }


        if (!eq && opt_bool && opt_not && !opt_all) {
            continue;
        }

        if ((!opt_bool && eq == !opt_not) || (opt_bool && (eq || opt_all))) {

            Jim_Obj *resultObj;

            if (opt_bool) {
                resultObj = Jim_NewIntObj(interp, eq ^ opt_not);
            }
            else if (!opt_inline) {
                resultObj = Jim_NewIntObj(interp, i);
            }
            else {
                resultObj = objPtr;
            }

            if (opt_all) {
                Jim_ListAppendElement(interp, listObjPtr, resultObj);
            }
            else {
                Jim_SetResult(interp, resultObj);
                goto done;
            }
        }
    }

    if (opt_all) {
        Jim_SetResult(interp, listObjPtr);
    }
    else {

        if (opt_bool) {
            Jim_SetResultBool(interp, opt_not);
        }
        else if (!opt_inline) {
            Jim_SetResultInt(interp, -1);
        }
    }

  done:
    if (commandObj) {
        Jim_DecrRefCount(interp, commandObj);
    }
    return rc;
}


static int Jim_LappendCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *listObjPtr;
    int new_obj = 0;
    int i;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "varName ?value value ...?");
        return JIM_ERR;
    }
    listObjPtr = Jim_GetVariable(interp, argv[1], JIM_UNSHARED);
    if (!listObjPtr) {

        listObjPtr = Jim_NewListObj(interp, NULL, 0);
        new_obj = 1;
    }
    else if (Jim_IsShared(listObjPtr)) {
        listObjPtr = Jim_DuplicateObj(interp, listObjPtr);
        new_obj = 1;
    }
    for (i = 2; i < argc; i++)
        Jim_ListAppendElement(interp, listObjPtr, argv[i]);
    if (Jim_SetVariable(interp, argv[1], listObjPtr) != JIM_OK) {
        if (new_obj)
            Jim_FreeNewObj(interp, listObjPtr);
        return JIM_ERR;
    }
    Jim_SetResult(interp, listObjPtr);
    return JIM_OK;
}


static int Jim_LinsertCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int idx, len;
    Jim_Obj *listPtr;

    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "list index ?element ...?");
        return JIM_ERR;
    }
    listPtr = argv[1];
    if (Jim_IsShared(listPtr))
        listPtr = Jim_DuplicateObj(interp, listPtr);
    if (Jim_GetIndex(interp, argv[2], &idx) != JIM_OK)
        goto err;
    len = Jim_ListLength(interp, listPtr);
    if (idx >= len)
        idx = len;
    else if (idx < 0)
        idx = len + idx + 1;
    Jim_ListInsertElements(interp, listPtr, idx, argc - 3, &argv[3]);
    Jim_SetResult(interp, listPtr);
    return JIM_OK;
  err:
    if (listPtr != argv[1]) {
        Jim_FreeNewObj(interp, listPtr);
    }
    return JIM_ERR;
}


static int Jim_LreplaceCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int first, last, len, rangeLen;
    Jim_Obj *listObj;
    Jim_Obj *newListObj;

    if (argc < 4) {
        Jim_WrongNumArgs(interp, 1, argv, "list first last ?element ...?");
        return JIM_ERR;
    }
    if (Jim_GetIndex(interp, argv[2], &first) != JIM_OK ||
        Jim_GetIndex(interp, argv[3], &last) != JIM_OK) {
        return JIM_ERR;
    }

    listObj = argv[1];
    len = Jim_ListLength(interp, listObj);

    first = JimRelToAbsIndex(len, first);
    last = JimRelToAbsIndex(len, last);
    JimRelToAbsRange(len, &first, &last, &rangeLen);


    if (first > len) {
        first = len;
    }


    newListObj = Jim_NewListObj(interp, listObj->internalRep.listValue.ele, first);


    ListInsertElements(newListObj, -1, argc - 4, argv + 4);


    ListInsertElements(newListObj, -1, len - first - rangeLen, listObj->internalRep.listValue.ele + first + rangeLen);

    Jim_SetResult(interp, newListObj);
    return JIM_OK;
}


static int Jim_LsetCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "listVar ?index...? newVal");
        return JIM_ERR;
    }
    else if (argc == 3) {

        if (Jim_SetVariable(interp, argv[1], argv[2]) != JIM_OK)
            return JIM_ERR;
        Jim_SetResult(interp, argv[2]);
        return JIM_OK;
    }
    return Jim_ListSetIndex(interp, argv[1], argv + 2, argc - 3, argv[argc - 1]);
}


static int Jim_LsortCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const argv[])
{
    static const char * const options[] = {
        "-ascii", "-nocase", "-increasing", "-decreasing", "-command", "-integer", "-real", "-index", "-unique", NULL
    };
    enum
    { OPT_ASCII, OPT_NOCASE, OPT_INCREASING, OPT_DECREASING, OPT_COMMAND, OPT_INTEGER, OPT_REAL, OPT_INDEX, OPT_UNIQUE };
    Jim_Obj *resObj;
    int i;
    int retCode;
    int shared;

    struct lsort_info info;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "?options? list");
        return JIM_ERR;
    }

    info.type = JIM_LSORT_ASCII;
    info.order = 1;
    info.indexed = 0;
    info.unique = 0;
    info.command = NULL;
    info.interp = interp;

    for (i = 1; i < (argc - 1); i++) {
        int option;

        if (Jim_GetEnum(interp, argv[i], options, &option, NULL, JIM_ENUM_ABBREV | JIM_ERRMSG)
            != JIM_OK)
            return JIM_ERR;
        switch (option) {
            case OPT_ASCII:
                info.type = JIM_LSORT_ASCII;
                break;
            case OPT_NOCASE:
                info.type = JIM_LSORT_NOCASE;
                break;
            case OPT_INTEGER:
                info.type = JIM_LSORT_INTEGER;
                break;
            case OPT_REAL:
                info.type = JIM_LSORT_REAL;
                break;
            case OPT_INCREASING:
                info.order = 1;
                break;
            case OPT_DECREASING:
                info.order = -1;
                break;
            case OPT_UNIQUE:
                info.unique = 1;
                break;
            case OPT_COMMAND:
                if (i >= (argc - 2)) {
                    Jim_SetResultString(interp, "\"-command\" option must be followed by comparison command", -1);
                    return JIM_ERR;
                }
                info.type = JIM_LSORT_COMMAND;
                info.command = argv[i + 1];
                i++;
                break;
            case OPT_INDEX:
                if (i >= (argc - 2)) {
                    Jim_SetResultString(interp, "\"-index\" option must be followed by list index", -1);
                    return JIM_ERR;
                }
                if (Jim_GetIndex(interp, argv[i + 1], &info.index) != JIM_OK) {
                    return JIM_ERR;
                }
                info.indexed = 1;
                i++;
                break;
        }
    }
    resObj = argv[argc - 1];
    if ((shared = Jim_IsShared(resObj)))
        resObj = Jim_DuplicateObj(interp, resObj);
    retCode = ListSortElements(interp, resObj, &info);
    if (retCode == JIM_OK) {
        Jim_SetResult(interp, resObj);
    }
    else if (shared) {
        Jim_FreeNewObj(interp, resObj);
    }
    return retCode;
}


static int Jim_AppendCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *stringObjPtr;
    int i;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "varName ?value ...?");
        return JIM_ERR;
    }
    if (argc == 2) {
        stringObjPtr = Jim_GetVariable(interp, argv[1], JIM_ERRMSG);
        if (!stringObjPtr)
            return JIM_ERR;
    }
    else {
        int new_obj = 0;
        stringObjPtr = Jim_GetVariable(interp, argv[1], JIM_UNSHARED);
        if (!stringObjPtr) {

            stringObjPtr = Jim_NewEmptyStringObj(interp);
            new_obj = 1;
        }
        else if (Jim_IsShared(stringObjPtr)) {
            new_obj = 1;
            stringObjPtr = Jim_DuplicateObj(interp, stringObjPtr);
        }
        for (i = 2; i < argc; i++) {
            Jim_AppendObj(interp, stringObjPtr, argv[i]);
        }
        if (Jim_SetVariable(interp, argv[1], stringObjPtr) != JIM_OK) {
            if (new_obj) {
                Jim_FreeNewObj(interp, stringObjPtr);
            }
            return JIM_ERR;
        }
    }
    Jim_SetResult(interp, stringObjPtr);
    return JIM_OK;
}



static int Jim_DebugCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
#if !defined(JIM_DEBUG_COMMAND)
    Jim_SetResultString(interp, "unsupported", -1);
    return JIM_ERR;
#endif
}


static int Jim_EvalCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int rc;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "arg ?arg ...?");
        return JIM_ERR;
    }

    if (argc == 2) {
        rc = Jim_EvalObj(interp, argv[1]);
    }
    else {
        rc = Jim_EvalObj(interp, Jim_ConcatObj(interp, argc - 1, argv + 1));
    }

    if (rc == JIM_ERR) {

        interp->addStackTrace++;
    }
    return rc;
}


static int Jim_UplevelCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc >= 2) {
        int retcode;
        Jim_CallFrame *savedCallFrame, *targetCallFrame;
        const char *str;


        savedCallFrame = interp->framePtr;


        str = Jim_String(argv[1]);
        if ((str[0] >= '0' && str[0] <= '9') || str[0] == '#') {
            targetCallFrame = Jim_GetCallFrameByLevel(interp, argv[1]);
            argc--;
            argv++;
        }
        else {
            targetCallFrame = Jim_GetCallFrameByLevel(interp, NULL);
        }
        if (targetCallFrame == NULL) {
            return JIM_ERR;
        }
        if (argc < 2) {
            Jim_WrongNumArgs(interp, 1, argv - 1, "?level? command ?arg ...?");
            return JIM_ERR;
        }

        interp->framePtr = targetCallFrame;
        if (argc == 2) {
            retcode = Jim_EvalObj(interp, argv[1]);
        }
        else {
            retcode = Jim_EvalObj(interp, Jim_ConcatObj(interp, argc - 1, argv + 1));
        }
        interp->framePtr = savedCallFrame;
        return retcode;
    }
    else {
        Jim_WrongNumArgs(interp, 1, argv, "?level? command ?arg ...?");
        return JIM_ERR;
    }
}


static int Jim_ExprCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int retcode;

    if (argc == 2) {
        retcode = Jim_EvalExpression(interp, argv[1]);
    }
    else if (argc > 2) {
        Jim_Obj *objPtr;

        objPtr = Jim_ConcatObj(interp, argc - 1, argv + 1);
        Jim_IncrRefCount(objPtr);
        retcode = Jim_EvalExpression(interp, objPtr);
        Jim_DecrRefCount(interp, objPtr);
    }
    else {
        Jim_WrongNumArgs(interp, 1, argv, "expression ?...?");
        return JIM_ERR;
    }
    if (retcode != JIM_OK)
        return retcode;
    return JIM_OK;
}


static int Jim_BreakCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }
    return JIM_BREAK;
}


static int Jim_ContinueCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }
    return JIM_CONTINUE;
}


static int Jim_ReturnCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;
    Jim_Obj *stackTraceObj = NULL;
    Jim_Obj *errorCodeObj = NULL;
    int returnCode = JIM_OK;
    long level = 1;

    for (i = 1; i < argc - 1; i += 2) {
        if (Jim_CompareStringImmediate(interp, argv[i], "-code")) {
            if (Jim_GetReturnCode(interp, argv[i + 1], &returnCode) == JIM_ERR) {
                return JIM_ERR;
            }
        }
        else if (Jim_CompareStringImmediate(interp, argv[i], "-errorinfo")) {
            stackTraceObj = argv[i + 1];
        }
        else if (Jim_CompareStringImmediate(interp, argv[i], "-errorcode")) {
            errorCodeObj = argv[i + 1];
        }
        else if (Jim_CompareStringImmediate(interp, argv[i], "-level")) {
            if (Jim_GetLong(interp, argv[i + 1], &level) != JIM_OK || level < 0) {
                Jim_SetResultFormatted(interp, "bad level \"%#s\"", argv[i + 1]);
                return JIM_ERR;
            }
        }
        else {
            break;
        }
    }

    if (i != argc - 1 && i != argc) {
        Jim_WrongNumArgs(interp, 1, argv,
            "?-code code? ?-errorinfo stacktrace? ?-level level? ?result?");
    }


    if (stackTraceObj && returnCode == JIM_ERR) {
        JimSetStackTrace(interp, stackTraceObj);
    }

    if (errorCodeObj && returnCode == JIM_ERR) {
        Jim_SetGlobalVariableStr(interp, "errorCode", errorCodeObj);
    }
    interp->returnCode = returnCode;
    interp->returnLevel = level;

    if (i == argc - 1) {
        Jim_SetResult(interp, argv[i]);
    }
    return JIM_RETURN;
}


static int Jim_TailcallCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (interp->framePtr->level == 0) {
        Jim_SetResultString(interp, "tailcall can only be called from a proc or lambda", -1);
        return JIM_ERR;
    }
    else if (argc >= 2) {

        Jim_CallFrame *cf = interp->framePtr->parent;

        Jim_Cmd *cmdPtr = Jim_GetCommand(interp, argv[1], JIM_ERRMSG);
        if (cmdPtr == NULL) {
            return JIM_ERR;
        }

        JimPanic((cf->tailcallCmd != NULL, "Already have a tailcallCmd"));


        JimIncrCmdRefCount(cmdPtr);
        cf->tailcallCmd = cmdPtr;


        JimPanic((cf->tailcallObj != NULL, "Already have a tailcallobj"));

        cf->tailcallObj = Jim_NewListObj(interp, argv + 1, argc - 1);
        Jim_IncrRefCount(cf->tailcallObj);


        return JIM_EVAL;
    }
    return JIM_OK;
}

static int JimAliasCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *cmdList;
    Jim_Obj *prefixListObj = Jim_CmdPrivData(interp);


    cmdList = Jim_DuplicateObj(interp, prefixListObj);
    Jim_ListInsertElements(interp, cmdList, Jim_ListLength(interp, cmdList), argc - 1, argv + 1);

    return JimEvalObjList(interp, cmdList);
}

static void JimAliasCmdDelete(Jim_Interp *interp, void *privData)
{
    Jim_Obj *prefixListObj = privData;
    Jim_DecrRefCount(interp, prefixListObj);
}

static int Jim_AliasCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *prefixListObj;
    const char *newname;

    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "newname command ?args ...?");
        return JIM_ERR;
    }

    prefixListObj = Jim_NewListObj(interp, argv + 2, argc - 2);
    Jim_IncrRefCount(prefixListObj);
    newname = Jim_String(argv[1]);
    if (newname[0] == ':' && newname[1] == ':') {
        while (*++newname == ':') {
        }
    }

    Jim_SetResult(interp, argv[1]);

    return Jim_CreateCommand(interp, newname, JimAliasCmd, prefixListObj, JimAliasCmdDelete);
}


static int Jim_ProcCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Cmd *cmd;

    if (argc != 4 && argc != 5) {
        Jim_WrongNumArgs(interp, 1, argv, "name arglist ?statics? body");
        return JIM_ERR;
    }

    if (JimValidName(interp, "procedure", argv[1]) != JIM_OK) {
        return JIM_ERR;
    }

    if (argc == 4) {
        cmd = JimCreateProcedureCmd(interp, argv[2], NULL, argv[3], NULL);
    }
    else {
        cmd = JimCreateProcedureCmd(interp, argv[2], argv[3], argv[4], NULL);
    }

    if (cmd) {

        Jim_Obj *qualifiedCmdNameObj;
        const char *cmdname = JimQualifyName(interp, Jim_String(argv[1]), &qualifiedCmdNameObj);

        JimCreateCommand(interp, cmdname, cmd);


        JimUpdateProcNamespace(interp, cmd, cmdname);

        JimFreeQualifiedName(interp, qualifiedCmdNameObj);


        Jim_SetResult(interp, argv[1]);
        return JIM_OK;
    }
    return JIM_ERR;
}


static int Jim_LocalCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int retcode;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "cmd ?args ...?");
        return JIM_ERR;
    }


    interp->local++;
    retcode = Jim_EvalObjVector(interp, argc - 1, argv + 1);
    interp->local--;



    if (retcode == 0) {
        Jim_Obj *cmdNameObj = Jim_GetResult(interp);

        if (Jim_GetCommand(interp, cmdNameObj, JIM_ERRMSG) == NULL) {
            return JIM_ERR;
        }
        if (interp->framePtr->localCommands == NULL) {
            interp->framePtr->localCommands = Jim_Alloc(sizeof(*interp->framePtr->localCommands));
            Jim_InitStack(interp->framePtr->localCommands);
        }
        Jim_IncrRefCount(cmdNameObj);
        Jim_StackPush(interp->framePtr->localCommands, cmdNameObj);
    }

    return retcode;
}


static int Jim_UpcallCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "cmd ?args ...?");
        return JIM_ERR;
    }
    else {
        int retcode;

        Jim_Cmd *cmdPtr = Jim_GetCommand(interp, argv[1], JIM_ERRMSG);
        if (cmdPtr == NULL || !cmdPtr->isproc || !cmdPtr->prevCmd) {
            Jim_SetResultFormatted(interp, "no previous command: \"%#s\"", argv[1]);
            return JIM_ERR;
        }

        cmdPtr->u.proc.upcall++;
        JimIncrCmdRefCount(cmdPtr);


        retcode = Jim_EvalObjVector(interp, argc - 1, argv + 1);


        cmdPtr->u.proc.upcall--;
        JimDecrCmdRefCount(interp, cmdPtr);

        return retcode;
    }
}


static int Jim_ApplyCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "lambdaExpr ?arg ...?");
        return JIM_ERR;
    }
    else {
        int ret;
        Jim_Cmd *cmd;
        Jim_Obj *argListObjPtr;
        Jim_Obj *bodyObjPtr;
        Jim_Obj *nsObj = NULL;
        Jim_Obj **nargv;

        int len = Jim_ListLength(interp, argv[1]);
        if (len != 2 && len != 3) {
            Jim_SetResultFormatted(interp, "can't interpret \"%#s\" as a lambda expression", argv[1]);
            return JIM_ERR;
        }

        if (len == 3) {
#ifdef jim_ext_namespace

            nsObj = JimQualifyNameObj(interp, Jim_ListGetIndex(interp, argv[1], 2));
#else
            Jim_SetResultString(interp, "namespaces not enabled", -1);
            return JIM_ERR;
#endif
        }
        argListObjPtr = Jim_ListGetIndex(interp, argv[1], 0);
        bodyObjPtr = Jim_ListGetIndex(interp, argv[1], 1);

        cmd = JimCreateProcedureCmd(interp, argListObjPtr, NULL, bodyObjPtr, nsObj);

        if (cmd) {

            nargv = Jim_Alloc((argc - 2 + 1) * sizeof(*nargv));
            nargv[0] = Jim_NewStringObj(interp, "apply lambdaExpr", -1);
            Jim_IncrRefCount(nargv[0]);
            memcpy(&nargv[1], argv + 2, (argc - 2) * sizeof(*nargv));
            ret = JimCallProcedure(interp, cmd, argc - 2 + 1, nargv);
            Jim_DecrRefCount(interp, nargv[0]);
            Jim_Free(nargv);

            JimDecrCmdRefCount(interp, cmd);
            return ret;
        }
        return JIM_ERR;
    }
}



static int Jim_ConcatCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_SetResult(interp, Jim_ConcatObj(interp, argc - 1, argv + 1));
    return JIM_OK;
}


static int Jim_UpvarCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;
    Jim_CallFrame *targetCallFrame;


    if (argc > 3 && (argc % 2 == 0)) {
        targetCallFrame = Jim_GetCallFrameByLevel(interp, argv[1]);
        argc--;
        argv++;
    }
    else {
        targetCallFrame = Jim_GetCallFrameByLevel(interp, NULL);
    }
    if (targetCallFrame == NULL) {
        return JIM_ERR;
    }


    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "?level? otherVar localVar ?otherVar localVar ...?");
        return JIM_ERR;
    }


    for (i = 1; i < argc; i += 2) {
        if (Jim_SetVariableLink(interp, argv[i + 1], argv[i], targetCallFrame) != JIM_OK)
            return JIM_ERR;
    }
    return JIM_OK;
}


static int Jim_GlobalCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "varName ?varName ...?");
        return JIM_ERR;
    }

    if (interp->framePtr->level == 0)
        return JIM_OK;
    for (i = 1; i < argc; i++) {

        const char *name = Jim_String(argv[i]);
        if (name[0] != ':' || name[1] != ':') {
            if (Jim_SetVariableLink(interp, argv[i], argv[i], interp->topFramePtr) != JIM_OK)
                return JIM_ERR;
        }
    }
    return JIM_OK;
}

static Jim_Obj *JimStringMap(Jim_Interp *interp, Jim_Obj *mapListObjPtr,
    Jim_Obj *objPtr, int nocase)
{
    int numMaps;
    const char *str, *noMatchStart = NULL;
    int strLen, i;
    Jim_Obj *resultObjPtr;

    numMaps = Jim_ListLength(interp, mapListObjPtr);
    if (numMaps % 2) {
        Jim_SetResultString(interp, "list must contain an even number of elements", -1);
        return NULL;
    }

    str = Jim_String(objPtr);
    strLen = Jim_Utf8Length(interp, objPtr);


    resultObjPtr = Jim_NewStringObj(interp, "", 0);
    while (strLen) {
        for (i = 0; i < numMaps; i += 2) {
            Jim_Obj *eachObjPtr;
            const char *k;
            int kl;

            eachObjPtr = Jim_ListGetIndex(interp, mapListObjPtr, i);
            k = Jim_String(eachObjPtr);
            kl = Jim_Utf8Length(interp, eachObjPtr);

            if (strLen >= kl && kl) {
                int rc;
                rc = JimStringCompareLen(str, k, kl, nocase);
                if (rc == 0) {
                    if (noMatchStart) {
                        Jim_AppendString(interp, resultObjPtr, noMatchStart, str - noMatchStart);
                        noMatchStart = NULL;
                    }
                    Jim_AppendObj(interp, resultObjPtr, Jim_ListGetIndex(interp, mapListObjPtr, i + 1));
                    str += utf8_index(str, kl);
                    strLen -= kl;
                    break;
                }
            }
        }
        if (i == numMaps) {
            int c;
            if (noMatchStart == NULL)
                noMatchStart = str;
            str += utf8_tounicode(str, &c);
            strLen--;
        }
    }
    if (noMatchStart) {
        Jim_AppendString(interp, resultObjPtr, noMatchStart, str - noMatchStart);
    }
    return resultObjPtr;
}


static int Jim_StringCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int len;
    int opt_case = 1;
    int option;
    static const char * const options[] = {
        "bytelength", "length", "compare", "match", "equal", "is", "byterange", "range", "replace",
        "map", "repeat", "reverse", "index", "first", "last", "cat",
        "trim", "trimleft", "trimright", "tolower", "toupper", "totitle", NULL
    };
    enum
    {
        OPT_BYTELENGTH, OPT_LENGTH, OPT_COMPARE, OPT_MATCH, OPT_EQUAL, OPT_IS, OPT_BYTERANGE, OPT_RANGE, OPT_REPLACE,
        OPT_MAP, OPT_REPEAT, OPT_REVERSE, OPT_INDEX, OPT_FIRST, OPT_LAST, OPT_CAT,
        OPT_TRIM, OPT_TRIMLEFT, OPT_TRIMRIGHT, OPT_TOLOWER, OPT_TOUPPER, OPT_TOTITLE
    };
    static const char * const nocase_options[] = {
        "-nocase", NULL
    };
    static const char * const nocase_length_options[] = {
        "-nocase", "-length", NULL
    };

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "option ?arguments ...?");
        return JIM_ERR;
    }
    if (Jim_GetEnum(interp, argv[1], options, &option, NULL,
            JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK)
        return Jim_CheckShowCommands(interp, argv[1], options);

    switch (option) {
        case OPT_LENGTH:
        case OPT_BYTELENGTH:
            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "string");
                return JIM_ERR;
            }
            if (option == OPT_LENGTH) {
                len = Jim_Utf8Length(interp, argv[2]);
            }
            else {
                len = Jim_Length(argv[2]);
            }
            Jim_SetResultInt(interp, len);
            return JIM_OK;

        case OPT_CAT:{
                Jim_Obj *objPtr;
                if (argc == 3) {

                    objPtr = argv[2];
                }
                else {
                    int i;

                    objPtr = Jim_NewStringObj(interp, "", 0);

                    for (i = 2; i < argc; i++) {
                        Jim_AppendObj(interp, objPtr, argv[i]);
                    }
                }
                Jim_SetResult(interp, objPtr);
                return JIM_OK;
            }

        case OPT_COMPARE:
        case OPT_EQUAL:
            {

                long opt_length = -1;
                int n = argc - 4;
                int i = 2;
                while (n > 0) {
                    int subopt;
                    if (Jim_GetEnum(interp, argv[i++], nocase_length_options, &subopt, NULL,
                            JIM_ENUM_ABBREV) != JIM_OK) {
badcompareargs:
                        Jim_WrongNumArgs(interp, 2, argv, "?-nocase? ?-length int? string1 string2");
                        return JIM_ERR;
                    }
                    if (subopt == 0) {

                        opt_case = 0;
                        n--;
                    }
                    else {

                        if (n < 2) {
                            goto badcompareargs;
                        }
                        if (Jim_GetLong(interp, argv[i++], &opt_length) != JIM_OK) {
                            return JIM_ERR;
                        }
                        n -= 2;
                    }
                }
                if (n) {
                    goto badcompareargs;
                }
                argv += argc - 2;
                if (opt_length < 0 && option != OPT_COMPARE && opt_case) {

                    Jim_SetResultBool(interp, Jim_StringEqObj(argv[0], argv[1]));
                }
                else {
                    if (opt_length >= 0) {
                        n = JimStringCompareLen(Jim_String(argv[0]), Jim_String(argv[1]), opt_length, !opt_case);
                    }
                    else {
                        n = Jim_StringCompareObj(interp, argv[0], argv[1], !opt_case);
                    }
                    Jim_SetResultInt(interp, option == OPT_COMPARE ? n : n == 0);
                }
                return JIM_OK;
            }

        case OPT_MATCH:
            if (argc != 4 &&
                (argc != 5 ||
                    Jim_GetEnum(interp, argv[2], nocase_options, &opt_case, NULL,
                        JIM_ENUM_ABBREV) != JIM_OK)) {
                Jim_WrongNumArgs(interp, 2, argv, "?-nocase? pattern string");
                return JIM_ERR;
            }
            if (opt_case == 0) {
                argv++;
            }
            Jim_SetResultBool(interp, Jim_StringMatchObj(interp, argv[2], argv[3], !opt_case));
            return JIM_OK;

        case OPT_MAP:{
                Jim_Obj *objPtr;

                if (argc != 4 &&
                    (argc != 5 ||
                        Jim_GetEnum(interp, argv[2], nocase_options, &opt_case, NULL,
                            JIM_ENUM_ABBREV) != JIM_OK)) {
                    Jim_WrongNumArgs(interp, 2, argv, "?-nocase? mapList string");
                    return JIM_ERR;
                }

                if (opt_case == 0) {
                    argv++;
                }
                objPtr = JimStringMap(interp, argv[2], argv[3], !opt_case);
                if (objPtr == NULL) {
                    return JIM_ERR;
                }
                Jim_SetResult(interp, objPtr);
                return JIM_OK;
            }

        case OPT_RANGE:
        case OPT_BYTERANGE:{
                Jim_Obj *objPtr;

                if (argc != 5) {
                    Jim_WrongNumArgs(interp, 2, argv, "string first last");
                    return JIM_ERR;
                }
                if (option == OPT_RANGE) {
                    objPtr = Jim_StringRangeObj(interp, argv[2], argv[3], argv[4]);
                }
                else
                {
                    objPtr = Jim_StringByteRangeObj(interp, argv[2], argv[3], argv[4]);
                }

                if (objPtr == NULL) {
                    return JIM_ERR;
                }
                Jim_SetResult(interp, objPtr);
                return JIM_OK;
            }

        case OPT_REPLACE:{
                Jim_Obj *objPtr;

                if (argc != 5 && argc != 6) {
                    Jim_WrongNumArgs(interp, 2, argv, "string first last ?string?");
                    return JIM_ERR;
                }
                objPtr = JimStringReplaceObj(interp, argv[2], argv[3], argv[4], argc == 6 ? argv[5] : NULL);
                if (objPtr == NULL) {
                    return JIM_ERR;
                }
                Jim_SetResult(interp, objPtr);
                return JIM_OK;
            }


        case OPT_REPEAT:{
                Jim_Obj *objPtr;
                jim_wide count;

                if (argc != 4) {
                    Jim_WrongNumArgs(interp, 2, argv, "string count");
                    return JIM_ERR;
                }
                if (Jim_GetWide(interp, argv[3], &count) != JIM_OK) {
                    return JIM_ERR;
                }
                objPtr = Jim_NewStringObj(interp, "", 0);
                if (count > 0) {
                    while (count--) {
                        Jim_AppendObj(interp, objPtr, argv[2]);
                    }
                }
                Jim_SetResult(interp, objPtr);
                return JIM_OK;
            }

        case OPT_REVERSE:{
                char *buf, *p;
                const char *str;
                int i;

                if (argc != 3) {
                    Jim_WrongNumArgs(interp, 2, argv, "string");
                    return JIM_ERR;
                }

                str = Jim_GetString(argv[2], &len);
                buf = Jim_Alloc(len + 1);
                p = buf + len;
                *p = 0;
                for (i = 0; i < len; ) {
                    int c;
                    int l = utf8_tounicode(str, &c);
                    memcpy(p - l, str, l);
                    p -= l;
                    i += l;
                    str += l;
                }
                Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, buf, len));
                return JIM_OK;
            }

        case OPT_INDEX:{
                int idx;
                const char *str;

                if (argc != 4) {
                    Jim_WrongNumArgs(interp, 2, argv, "string index");
                    return JIM_ERR;
                }
                if (Jim_GetIndex(interp, argv[3], &idx) != JIM_OK) {
                    return JIM_ERR;
                }
                str = Jim_String(argv[2]);
                len = Jim_Utf8Length(interp, argv[2]);
                if (idx != INT_MIN && idx != INT_MAX) {
                    idx = JimRelToAbsIndex(len, idx);
                }
                if (idx < 0 || idx >= len || str == NULL) {
                    Jim_SetResultString(interp, "", 0);
                }
                else if (len == Jim_Length(argv[2])) {

                    Jim_SetResultString(interp, str + idx, 1);
                }
                else {
                    int c;
                    int i = utf8_index(str, idx);
                    Jim_SetResultString(interp, str + i, utf8_tounicode(str + i, &c));
                }
                return JIM_OK;
            }

        case OPT_FIRST:
        case OPT_LAST:{
                int idx = 0, l1, l2;
                const char *s1, *s2;

                if (argc != 4 && argc != 5) {
                    Jim_WrongNumArgs(interp, 2, argv, "subString string ?index?");
                    return JIM_ERR;
                }
                s1 = Jim_String(argv[2]);
                s2 = Jim_String(argv[3]);
                l1 = Jim_Utf8Length(interp, argv[2]);
                l2 = Jim_Utf8Length(interp, argv[3]);
                if (argc == 5) {
                    if (Jim_GetIndex(interp, argv[4], &idx) != JIM_OK) {
                        return JIM_ERR;
                    }
                    idx = JimRelToAbsIndex(l2, idx);
                }
                else if (option == OPT_LAST) {
                    idx = l2;
                }
                if (option == OPT_FIRST) {
                    Jim_SetResultInt(interp, JimStringFirst(s1, l1, s2, l2, idx));
                }
                else {
#ifdef JIM_UTF8
                    Jim_SetResultInt(interp, JimStringLastUtf8(s1, l1, s2, idx));
#else
                    Jim_SetResultInt(interp, JimStringLast(s1, l1, s2, idx));
#endif
                }
                return JIM_OK;
            }

        case OPT_TRIM:
        case OPT_TRIMLEFT:
        case OPT_TRIMRIGHT:{
                Jim_Obj *trimchars;

                if (argc != 3 && argc != 4) {
                    Jim_WrongNumArgs(interp, 2, argv, "string ?trimchars?");
                    return JIM_ERR;
                }
                trimchars = (argc == 4 ? argv[3] : NULL);
                if (option == OPT_TRIM) {
                    Jim_SetResult(interp, JimStringTrim(interp, argv[2], trimchars));
                }
                else if (option == OPT_TRIMLEFT) {
                    Jim_SetResult(interp, JimStringTrimLeft(interp, argv[2], trimchars));
                }
                else if (option == OPT_TRIMRIGHT) {
                    Jim_SetResult(interp, JimStringTrimRight(interp, argv[2], trimchars));
                }
                return JIM_OK;
            }

        case OPT_TOLOWER:
        case OPT_TOUPPER:
        case OPT_TOTITLE:
            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "string");
                return JIM_ERR;
            }
            if (option == OPT_TOLOWER) {
                Jim_SetResult(interp, JimStringToLower(interp, argv[2]));
            }
            else if (option == OPT_TOUPPER) {
                Jim_SetResult(interp, JimStringToUpper(interp, argv[2]));
            }
            else {
                Jim_SetResult(interp, JimStringToTitle(interp, argv[2]));
            }
            return JIM_OK;

        case OPT_IS:
            if (argc == 4 || (argc == 5 && Jim_CompareStringImmediate(interp, argv[3], "-strict"))) {
                return JimStringIs(interp, argv[argc - 1], argv[2], argc == 5);
            }
            Jim_WrongNumArgs(interp, 2, argv, "class ?-strict? str");
            return JIM_ERR;
    }
    return JIM_OK;
}


static int Jim_TimeCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    long i, count = 1;
    jim_wide start, elapsed;
    char buf[60];
    const char *fmt = "%" JIM_WIDE_MODIFIER " microseconds per iteration";

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "script ?count?");
        return JIM_ERR;
    }
    if (argc == 3) {
        if (Jim_GetLong(interp, argv[2], &count) != JIM_OK)
            return JIM_ERR;
    }
    if (count < 0)
        return JIM_OK;
    i = count;
    start = JimClock();
    while (i-- > 0) {
        int retval;

        retval = Jim_EvalObj(interp, argv[1]);
        if (retval != JIM_OK) {
            return retval;
        }
    }
    elapsed = JimClock() - start;
    sprintf(buf, fmt, count == 0 ? 0 : elapsed / count);
    Jim_SetResultString(interp, buf, -1);
    return JIM_OK;
}


static int Jim_ExitCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    long exitCode = 0;

    if (argc > 2) {
        Jim_WrongNumArgs(interp, 1, argv, "?exitCode?");
        return JIM_ERR;
    }
    if (argc == 2) {
        if (Jim_GetLong(interp, argv[1], &exitCode) != JIM_OK)
            return JIM_ERR;
    }
    interp->exitCode = exitCode;
    return JIM_EXIT;
}


static int Jim_CatchCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int exitCode = 0;
    int i;
    int sig = 0;


    jim_wide ignore_mask = (1 << JIM_EXIT) | (1 << JIM_EVAL) | (1 << JIM_SIGNAL);
    static const int max_ignore_code = sizeof(ignore_mask) * 8;

    Jim_SetGlobalVariableStr(interp, "errorCode", Jim_NewStringObj(interp, "NONE", -1));

    for (i = 1; i < argc - 1; i++) {
        const char *arg = Jim_String(argv[i]);
        jim_wide option;
        int ignore;


        if (strcmp(arg, "--") == 0) {
            i++;
            break;
        }
        if (*arg != '-') {
            break;
        }

        if (strncmp(arg, "-no", 3) == 0) {
            arg += 3;
            ignore = 1;
        }
        else {
            arg++;
            ignore = 0;
        }

        if (Jim_StringToWide(arg, &option, 10) != JIM_OK) {
            option = -1;
        }
        if (option < 0) {
            option = Jim_FindByName(arg, jimReturnCodes, jimReturnCodesSize);
        }
        if (option < 0) {
            goto wrongargs;
        }

        if (ignore) {
            ignore_mask |= ((jim_wide)1 << option);
        }
        else {
            ignore_mask &= (~((jim_wide)1 << option));
        }
    }

    argc -= i;
    if (argc < 1 || argc > 3) {
      wrongargs:
        Jim_WrongNumArgs(interp, 1, argv,
            "?-?no?code ... --? script ?resultVarName? ?optionVarName?");
        return JIM_ERR;
    }
    argv += i;

    if ((ignore_mask & (1 << JIM_SIGNAL)) == 0) {
        sig++;
    }

    interp->signal_level += sig;
    if (Jim_CheckSignal(interp)) {

        exitCode = JIM_SIGNAL;
    }
    else {
        exitCode = Jim_EvalObj(interp, argv[0]);

        interp->errorFlag = 0;
    }
    interp->signal_level -= sig;


    if (exitCode >= 0 && exitCode < max_ignore_code && (((unsigned jim_wide)1 << exitCode) & ignore_mask)) {

        return exitCode;
    }

    if (sig && exitCode == JIM_SIGNAL) {

        if (interp->signal_set_result) {
            interp->signal_set_result(interp, interp->sigmask);
        }
        else {
            Jim_SetResultInt(interp, interp->sigmask);
        }
        interp->sigmask = 0;
    }

    if (argc >= 2) {
        if (Jim_SetVariable(interp, argv[1], Jim_GetResult(interp)) != JIM_OK) {
            return JIM_ERR;
        }
        if (argc == 3) {
            Jim_Obj *optListObj = Jim_NewListObj(interp, NULL, 0);

            Jim_ListAppendElement(interp, optListObj, Jim_NewStringObj(interp, "-code", -1));
            Jim_ListAppendElement(interp, optListObj,
                Jim_NewIntObj(interp, exitCode == JIM_RETURN ? interp->returnCode : exitCode));
            Jim_ListAppendElement(interp, optListObj, Jim_NewStringObj(interp, "-level", -1));
            Jim_ListAppendElement(interp, optListObj, Jim_NewIntObj(interp, interp->returnLevel));
            if (exitCode == JIM_ERR) {
                Jim_Obj *errorCode;
                Jim_ListAppendElement(interp, optListObj, Jim_NewStringObj(interp, "-errorinfo",
                    -1));
                Jim_ListAppendElement(interp, optListObj, interp->stackTrace);

                errorCode = Jim_GetGlobalVariableStr(interp, "errorCode", JIM_NONE);
                if (errorCode) {
                    Jim_ListAppendElement(interp, optListObj, Jim_NewStringObj(interp, "-errorcode", -1));
                    Jim_ListAppendElement(interp, optListObj, errorCode);
                }
            }
            if (Jim_SetVariable(interp, argv[2], optListObj) != JIM_OK) {
                return JIM_ERR;
            }
        }
    }
    Jim_SetResultInt(interp, exitCode);
    return JIM_OK;
}



static int Jim_RenameCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "oldName newName");
        return JIM_ERR;
    }

    if (JimValidName(interp, "new procedure", argv[2])) {
        return JIM_ERR;
    }

    return Jim_RenameCommand(interp, Jim_String(argv[1]), Jim_String(argv[2]));
}

#define JIM_DICTMATCH_KEYS 0x0001
#define JIM_DICTMATCH_VALUES 0x002

int Jim_DictMatchTypes(Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *patternObj, int match_type, int return_types)
{
    Jim_HashEntry *he;
    Jim_Obj *listObjPtr;
    Jim_HashTableIterator htiter;

    if (SetDictFromAny(interp, objPtr) != JIM_OK) {
        return JIM_ERR;
    }

    listObjPtr = Jim_NewListObj(interp, NULL, 0);

    JimInitHashTableIterator(objPtr->internalRep.ptr, &htiter);
    while ((he = Jim_NextHashEntry(&htiter)) != NULL) {
        if (patternObj) {
            Jim_Obj *matchObj = (match_type == JIM_DICTMATCH_KEYS) ? (Jim_Obj *)he->key : Jim_GetHashEntryVal(he);
            if (!JimGlobMatch(Jim_String(patternObj), Jim_String(matchObj), 0)) {

                continue;
            }
        }
        if (return_types & JIM_DICTMATCH_KEYS) {
            Jim_ListAppendElement(interp, listObjPtr, (Jim_Obj *)he->key);
        }
        if (return_types & JIM_DICTMATCH_VALUES) {
            Jim_ListAppendElement(interp, listObjPtr, Jim_GetHashEntryVal(he));
        }
    }

    Jim_SetResult(interp, listObjPtr);
    return JIM_OK;
}

int Jim_DictSize(Jim_Interp *interp, Jim_Obj *objPtr)
{
    if (SetDictFromAny(interp, objPtr) != JIM_OK) {
        return -1;
    }
    return ((Jim_HashTable *)objPtr->internalRep.ptr)->used;
}

Jim_Obj *Jim_DictMerge(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    Jim_Obj *objPtr = Jim_NewDictObj(interp, NULL, 0);
    int i;

    JimPanic((objc == 0, "Jim_DictMerge called with objc=0"));



    for (i = 0; i < objc; i++) {
        Jim_HashTable *ht;
        Jim_HashTableIterator htiter;
        Jim_HashEntry *he;

        if (SetDictFromAny(interp, objv[i]) != JIM_OK) {
            Jim_FreeNewObj(interp, objPtr);
            return NULL;
        }
        ht = objv[i]->internalRep.ptr;
        JimInitHashTableIterator(ht, &htiter);
        while ((he = Jim_NextHashEntry(&htiter)) != NULL) {
            Jim_ReplaceHashEntry(objPtr->internalRep.ptr, Jim_GetHashEntryKey(he), Jim_GetHashEntryVal(he));
        }
    }
    return objPtr;
}

int Jim_DictInfo(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_HashTable *ht;
    unsigned int i;
    char buffer[100];
    int sum = 0;
    int nonzero_count = 0;
    Jim_Obj *output;
    int bucket_counts[11] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    if (SetDictFromAny(interp, objPtr) != JIM_OK) {
        return JIM_ERR;
    }

    ht = (Jim_HashTable *)objPtr->internalRep.ptr;


    snprintf(buffer, sizeof(buffer), "%d entries in table, %d buckets\n", ht->used, ht->size);
    output = Jim_NewStringObj(interp, buffer, -1);

    for (i = 0; i < ht->size; i++) {
        Jim_HashEntry *he = ht->table[i];
        int entries = 0;
        while (he) {
            entries++;
            he = he->next;
        }
        if (entries > 9) {
            bucket_counts[10]++;
        }
        else {
            bucket_counts[entries]++;
        }
        if (entries) {
            sum += entries;
            nonzero_count++;
        }
    }
    for (i = 0; i < 10; i++) {
        snprintf(buffer, sizeof(buffer), "number of buckets with %d entries: %d\n", i, bucket_counts[i]);
        Jim_AppendString(interp, output, buffer, -1);
    }
    snprintf(buffer, sizeof(buffer), "number of buckets with 10 or more entries: %d\n", bucket_counts[10]);
    Jim_AppendString(interp, output, buffer, -1);
    snprintf(buffer, sizeof(buffer), "average search distance for entry: %.1f", nonzero_count ? (double)sum / nonzero_count : 0.0);
    Jim_AppendString(interp, output, buffer, -1);
    Jim_SetResult(interp, output);
    return JIM_OK;
}

static int Jim_EvalEnsemble(Jim_Interp *interp, const char *basecmd, const char *subcmd, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *prefixObj = Jim_NewStringObj(interp, basecmd, -1);

    Jim_AppendString(interp, prefixObj, " ", 1);
    Jim_AppendString(interp, prefixObj, subcmd, -1);

    return Jim_EvalObjPrefix(interp, prefixObj, argc, argv);
}

static int JimDictWith(Jim_Interp *interp, Jim_Obj *dictVarName, Jim_Obj *const *keyv, int keyc, Jim_Obj *scriptObj)
{
    int i;
    Jim_Obj *objPtr;
    Jim_Obj *dictObj;
    Jim_Obj **dictValues;
    int len;
    int ret = JIM_OK;


    dictObj = Jim_GetVariable(interp, dictVarName, JIM_ERRMSG);
    if (dictObj == NULL || Jim_DictKeysVector(interp, dictObj, keyv, keyc, &objPtr, JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }

    if (Jim_DictPairs(interp, objPtr, &dictValues, &len) == JIM_ERR) {
        return JIM_ERR;
    }
    for (i = 0; i < len; i += 2) {
        if (Jim_SetVariable(interp, dictValues[i], dictValues[i + 1]) == JIM_ERR) {
            Jim_Free(dictValues);
            return JIM_ERR;
        }
    }


    if (Jim_Length(scriptObj)) {
        ret = Jim_EvalObj(interp, scriptObj);


        if (ret == JIM_OK && Jim_GetVariable(interp, dictVarName, 0) != NULL) {

            Jim_Obj **newkeyv = Jim_Alloc(sizeof(*newkeyv) * (keyc + 1));
            for (i = 0; i < keyc; i++) {
                newkeyv[i] = keyv[i];
            }

            for (i = 0; i < len; i += 2) {

                objPtr = Jim_GetVariable(interp, dictValues[i], 0);
                newkeyv[keyc] = dictValues[i];
                Jim_SetDictKeysVector(interp, dictVarName, newkeyv, keyc + 1, objPtr, 0);
            }
            Jim_Free(newkeyv);
        }
    }

    Jim_Free(dictValues);

    return ret;
}


static int Jim_DictCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;
    int types = JIM_DICTMATCH_KEYS;
    int option;
    static const char * const options[] = {
        "create", "get", "set", "unset", "exists", "keys", "size", "info",
        "merge", "with", "append", "lappend", "incr", "remove", "values", "for",
        "replace", "update", NULL
    };
    enum
    {
        OPT_CREATE, OPT_GET, OPT_SET, OPT_UNSET, OPT_EXISTS, OPT_KEYS, OPT_SIZE, OPT_INFO,
        OPT_MERGE, OPT_WITH, OPT_APPEND, OPT_LAPPEND, OPT_INCR, OPT_REMOVE, OPT_VALUES, OPT_FOR,
        OPT_REPLACE, OPT_UPDATE,
    };

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "subcommand ?arguments ...?");
        return JIM_ERR;
    }

    if (Jim_GetEnum(interp, argv[1], options, &option, "subcommand", JIM_ERRMSG) != JIM_OK) {
        return Jim_CheckShowCommands(interp, argv[1], options);
    }

    switch (option) {
        case OPT_GET:
            if (argc < 3) {
                Jim_WrongNumArgs(interp, 2, argv, "dictionary ?key ...?");
                return JIM_ERR;
            }
            if (Jim_DictKeysVector(interp, argv[2], argv + 3, argc - 3, &objPtr,
                    JIM_ERRMSG) != JIM_OK) {
                return JIM_ERR;
            }
            Jim_SetResult(interp, objPtr);
            return JIM_OK;

        case OPT_SET:
            if (argc < 5) {
                Jim_WrongNumArgs(interp, 2, argv, "varName key ?key ...? value");
                return JIM_ERR;
            }
            return Jim_SetDictKeysVector(interp, argv[2], argv + 3, argc - 4, argv[argc - 1], JIM_ERRMSG);

        case OPT_EXISTS:
            if (argc < 4) {
                Jim_WrongNumArgs(interp, 2, argv, "dictionary key ?key ...?");
                return JIM_ERR;
            }
            else {
                int rc = Jim_DictKeysVector(interp, argv[2], argv + 3, argc - 3, &objPtr, JIM_ERRMSG);
                if (rc < 0) {
                    return JIM_ERR;
                }
                Jim_SetResultBool(interp,  rc == JIM_OK);
                return JIM_OK;
            }

        case OPT_UNSET:
            if (argc < 4) {
                Jim_WrongNumArgs(interp, 2, argv, "varName key ?key ...?");
                return JIM_ERR;
            }
            if (Jim_SetDictKeysVector(interp, argv[2], argv + 3, argc - 3, NULL, 0) != JIM_OK) {
                return JIM_ERR;
            }
            return JIM_OK;

        case OPT_VALUES:
            types = JIM_DICTMATCH_VALUES;

        case OPT_KEYS:
            if (argc != 3 && argc != 4) {
                Jim_WrongNumArgs(interp, 2, argv, "dictionary ?pattern?");
                return JIM_ERR;
            }
            return Jim_DictMatchTypes(interp, argv[2], argc == 4 ? argv[3] : NULL, types, types);

        case OPT_SIZE:
            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "dictionary");
                return JIM_ERR;
            }
            else if (Jim_DictSize(interp, argv[2]) < 0) {
                return JIM_ERR;
            }
            Jim_SetResultInt(interp, Jim_DictSize(interp, argv[2]));
            return JIM_OK;

        case OPT_MERGE:
            if (argc == 2) {
                return JIM_OK;
            }
            objPtr = Jim_DictMerge(interp, argc - 2, argv + 2);
            if (objPtr == NULL) {
                return JIM_ERR;
            }
            Jim_SetResult(interp, objPtr);
            return JIM_OK;

        case OPT_UPDATE:
            if (argc < 6 || argc % 2) {

                argc = 2;
            }
            break;

        case OPT_CREATE:
            if (argc % 2) {
                Jim_WrongNumArgs(interp, 2, argv, "?key value ...?");
                return JIM_ERR;
            }
            objPtr = Jim_NewDictObj(interp, argv + 2, argc - 2);
            Jim_SetResult(interp, objPtr);
            return JIM_OK;

        case OPT_INFO:
            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "dictionary");
                return JIM_ERR;
            }
            return Jim_DictInfo(interp, argv[2]);

        case OPT_WITH:
            if (argc < 4) {
                Jim_WrongNumArgs(interp, 2, argv, "dictVar ?key ...? script");
                return JIM_ERR;
            }
            return JimDictWith(interp, argv[2], argv + 3, argc - 4, argv[argc - 1]);
    }

    return Jim_EvalEnsemble(interp, "dict", options[option], argc - 2, argv + 2);
}


static int Jim_SubstCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    static const char * const options[] = {
        "-nobackslashes", "-nocommands", "-novariables", NULL
    };
    enum
    { OPT_NOBACKSLASHES, OPT_NOCOMMANDS, OPT_NOVARIABLES };
    int i;
    int flags = JIM_SUBST_FLAG;
    Jim_Obj *objPtr;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "?options? string");
        return JIM_ERR;
    }
    for (i = 1; i < (argc - 1); i++) {
        int option;

        if (Jim_GetEnum(interp, argv[i], options, &option, NULL,
                JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
            return JIM_ERR;
        }
        switch (option) {
            case OPT_NOBACKSLASHES:
                flags |= JIM_SUBST_NOESC;
                break;
            case OPT_NOCOMMANDS:
                flags |= JIM_SUBST_NOCMD;
                break;
            case OPT_NOVARIABLES:
                flags |= JIM_SUBST_NOVAR;
                break;
        }
    }
    if (Jim_SubstObj(interp, argv[argc - 1], &objPtr, flags) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}


static int Jim_InfoCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int cmd;
    Jim_Obj *objPtr;
    int mode = 0;

    static const char * const commands[] = {
        "body", "statics", "commands", "procs", "channels", "exists", "globals", "level", "frame", "locals",
        "vars", "version", "patchlevel", "complete", "args", "hostname",
        "script", "source", "stacktrace", "nameofexecutable", "returncodes",
        "references", "alias", NULL
    };
    enum
    { INFO_BODY, INFO_STATICS, INFO_COMMANDS, INFO_PROCS, INFO_CHANNELS, INFO_EXISTS, INFO_GLOBALS, INFO_LEVEL,
        INFO_FRAME, INFO_LOCALS, INFO_VARS, INFO_VERSION, INFO_PATCHLEVEL, INFO_COMPLETE, INFO_ARGS,
        INFO_HOSTNAME, INFO_SCRIPT, INFO_SOURCE, INFO_STACKTRACE, INFO_NAMEOFEXECUTABLE,
        INFO_RETURNCODES, INFO_REFERENCES, INFO_ALIAS,
    };

#ifdef jim_ext_namespace
    int nons = 0;

    if (argc > 2 && Jim_CompareStringImmediate(interp, argv[1], "-nons")) {

        argc--;
        argv++;
        nons = 1;
    }
#endif

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "subcommand ?args ...?");
        return JIM_ERR;
    }
    if (Jim_GetEnum(interp, argv[1], commands, &cmd, "subcommand", JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
        return Jim_CheckShowCommands(interp, argv[1], commands);
    }


    switch (cmd) {
        case INFO_EXISTS:
            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "varName");
                return JIM_ERR;
            }
            Jim_SetResultBool(interp, Jim_GetVariable(interp, argv[2], 0) != NULL);
            break;

        case INFO_ALIAS:{
            Jim_Cmd *cmdPtr;

            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "command");
                return JIM_ERR;
            }
            if ((cmdPtr = Jim_GetCommand(interp, argv[2], JIM_ERRMSG)) == NULL) {
                return JIM_ERR;
            }
            if (cmdPtr->isproc || cmdPtr->u.native.cmdProc != JimAliasCmd) {
                Jim_SetResultFormatted(interp, "command \"%#s\" is not an alias", argv[2]);
                return JIM_ERR;
            }
            Jim_SetResult(interp, (Jim_Obj *)cmdPtr->u.native.privData);
            return JIM_OK;
        }

        case INFO_CHANNELS:
            mode++;
#ifndef jim_ext_aio
            Jim_SetResultString(interp, "aio not enabled", -1);
            return JIM_ERR;
#endif

        case INFO_PROCS:
            mode++;

        case INFO_COMMANDS:

            if (argc != 2 && argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "?pattern?");
                return JIM_ERR;
            }
#ifdef jim_ext_namespace
            if (!nons) {
                if (Jim_Length(interp->framePtr->nsObj) || (argc == 3 && JimGlobMatch("::*", Jim_String(argv[2]), 0))) {
                    return Jim_EvalPrefix(interp, "namespace info", argc - 1, argv + 1);
                }
            }
#endif
            Jim_SetResult(interp, JimCommandsList(interp, (argc == 3) ? argv[2] : NULL, mode));
            break;

        case INFO_VARS:
            mode++;

        case INFO_LOCALS:
            mode++;

        case INFO_GLOBALS:

            if (argc != 2 && argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "?pattern?");
                return JIM_ERR;
            }
#ifdef jim_ext_namespace
            if (!nons) {
                if (Jim_Length(interp->framePtr->nsObj) || (argc == 3 && JimGlobMatch("::*", Jim_String(argv[2]), 0))) {
                    return Jim_EvalPrefix(interp, "namespace info", argc - 1, argv + 1);
                }
            }
#endif
            Jim_SetResult(interp, JimVariablesList(interp, argc == 3 ? argv[2] : NULL, mode));
            break;

        case INFO_SCRIPT:
            if (argc != 2) {
                Jim_WrongNumArgs(interp, 2, argv, "");
                return JIM_ERR;
            }
            Jim_SetResult(interp, JimGetScript(interp, interp->currentScriptObj)->fileNameObj);
            break;

        case INFO_SOURCE:{
                jim_wide line;
                Jim_Obj *resObjPtr;
                Jim_Obj *fileNameObj;

                if (argc != 3 && argc != 5) {
                    Jim_WrongNumArgs(interp, 2, argv, "source ?filename line?");
                    return JIM_ERR;
                }
                if (argc == 5) {
                    if (Jim_GetWide(interp, argv[4], &line) != JIM_OK) {
                        return JIM_ERR;
                    }
                    resObjPtr = Jim_NewStringObj(interp, Jim_String(argv[2]), Jim_Length(argv[2]));
                    JimSetSourceInfo(interp, resObjPtr, argv[3], line);
                }
                else {
                    if (argv[2]->typePtr == &sourceObjType) {
                        fileNameObj = argv[2]->internalRep.sourceValue.fileNameObj;
                        line = argv[2]->internalRep.sourceValue.lineNumber;
                    }
                    else if (argv[2]->typePtr == &scriptObjType) {
                        ScriptObj *script = JimGetScript(interp, argv[2]);
                        fileNameObj = script->fileNameObj;
                        line = script->firstline;
                    }
                    else {
                        fileNameObj = interp->emptyObj;
                        line = 1;
                    }
                    resObjPtr = Jim_NewListObj(interp, NULL, 0);
                    Jim_ListAppendElement(interp, resObjPtr, fileNameObj);
                    Jim_ListAppendElement(interp, resObjPtr, Jim_NewIntObj(interp, line));
                }
                Jim_SetResult(interp, resObjPtr);
                break;
            }

        case INFO_STACKTRACE:
            Jim_SetResult(interp, interp->stackTrace);
            break;

        case INFO_LEVEL:
        case INFO_FRAME:
            switch (argc) {
                case 2:
                    Jim_SetResultInt(interp, interp->framePtr->level);
                    break;

                case 3:
                    if (JimInfoLevel(interp, argv[2], &objPtr, cmd == INFO_LEVEL) != JIM_OK) {
                        return JIM_ERR;
                    }
                    Jim_SetResult(interp, objPtr);
                    break;

                default:
                    Jim_WrongNumArgs(interp, 2, argv, "?levelNum?");
                    return JIM_ERR;
            }
            break;

        case INFO_BODY:
        case INFO_STATICS:
        case INFO_ARGS:{
                Jim_Cmd *cmdPtr;

                if (argc != 3) {
                    Jim_WrongNumArgs(interp, 2, argv, "procname");
                    return JIM_ERR;
                }
                if ((cmdPtr = Jim_GetCommand(interp, argv[2], JIM_ERRMSG)) == NULL) {
                    return JIM_ERR;
                }
                if (!cmdPtr->isproc) {
                    Jim_SetResultFormatted(interp, "command \"%#s\" is not a procedure", argv[2]);
                    return JIM_ERR;
                }
                switch (cmd) {
                    case INFO_BODY:
                        Jim_SetResult(interp, cmdPtr->u.proc.bodyObjPtr);
                        break;
                    case INFO_ARGS:
                        Jim_SetResult(interp, cmdPtr->u.proc.argListObjPtr);
                        break;
                    case INFO_STATICS:
                        if (cmdPtr->u.proc.staticVars) {
                            Jim_SetResult(interp, JimHashtablePatternMatch(interp, cmdPtr->u.proc.staticVars,
                                NULL, JimVariablesMatch, JIM_VARLIST_LOCALS | JIM_VARLIST_VALUES));
                        }
                        break;
                }
                break;
            }

        case INFO_VERSION:
        case INFO_PATCHLEVEL:{
                char buf[(JIM_INTEGER_SPACE * 2) + 1];

                sprintf(buf, "%d.%d", JIM_VERSION / 100, JIM_VERSION % 100);
                Jim_SetResultString(interp, buf, -1);
                break;
            }

        case INFO_COMPLETE:
            if (argc != 3 && argc != 4) {
                Jim_WrongNumArgs(interp, 2, argv, "script ?missing?");
                return JIM_ERR;
            }
            else {
                char missing;

                Jim_SetResultBool(interp, Jim_ScriptIsComplete(interp, argv[2], &missing));
                if (missing != ' ' && argc == 4) {
                    Jim_SetVariable(interp, argv[3], Jim_NewStringObj(interp, &missing, 1));
                }
            }
            break;

        case INFO_HOSTNAME:

            return Jim_Eval(interp, "os.gethostname");

        case INFO_NAMEOFEXECUTABLE:

            return Jim_Eval(interp, "{info nameofexecutable}");

        case INFO_RETURNCODES:
            if (argc == 2) {
                int i;
                Jim_Obj *listObjPtr = Jim_NewListObj(interp, NULL, 0);

                for (i = 0; jimReturnCodes[i]; i++) {
                    Jim_ListAppendElement(interp, listObjPtr, Jim_NewIntObj(interp, i));
                    Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp,
                            jimReturnCodes[i], -1));
                }

                Jim_SetResult(interp, listObjPtr);
            }
            else if (argc == 3) {
                long code;
                const char *name;

                if (Jim_GetLong(interp, argv[2], &code) != JIM_OK) {
                    return JIM_ERR;
                }
                name = Jim_ReturnCode(code);
                if (*name == '?') {
                    Jim_SetResultInt(interp, code);
                }
                else {
                    Jim_SetResultString(interp, name, -1);
                }
            }
            else {
                Jim_WrongNumArgs(interp, 2, argv, "?code?");
                return JIM_ERR;
            }
            break;
        case INFO_REFERENCES:
#ifdef JIM_REFERENCES
            return JimInfoReferences(interp, argc, argv);
#else
            Jim_SetResultString(interp, "not supported", -1);
            return JIM_ERR;
#endif
    }
    return JIM_OK;
}


static int Jim_ExistsCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;
    int result = 0;

    static const char * const options[] = {
        "-command", "-proc", "-alias", "-var", NULL
    };
    enum
    {
        OPT_COMMAND, OPT_PROC, OPT_ALIAS, OPT_VAR
    };
    int option;

    if (argc == 2) {
        option = OPT_VAR;
        objPtr = argv[1];
    }
    else if (argc == 3) {
        if (Jim_GetEnum(interp, argv[1], options, &option, NULL, JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
            return JIM_ERR;
        }
        objPtr = argv[2];
    }
    else {
        Jim_WrongNumArgs(interp, 1, argv, "?option? name");
        return JIM_ERR;
    }

    if (option == OPT_VAR) {
        result = Jim_GetVariable(interp, objPtr, 0) != NULL;
    }
    else {

        Jim_Cmd *cmd = Jim_GetCommand(interp, objPtr, JIM_NONE);

        if (cmd) {
            switch (option) {
            case OPT_COMMAND:
                result = 1;
                break;

            case OPT_ALIAS:
                result = cmd->isproc == 0 && cmd->u.native.cmdProc == JimAliasCmd;
                break;

            case OPT_PROC:
                result = cmd->isproc;
                break;
            }
        }
    }
    Jim_SetResultBool(interp, result);
    return JIM_OK;
}


static int Jim_SplitCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *str, *splitChars, *noMatchStart;
    int splitLen, strLen;
    Jim_Obj *resObjPtr;
    int c;
    int len;

    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "string ?splitChars?");
        return JIM_ERR;
    }

    str = Jim_GetString(argv[1], &len);
    if (len == 0) {
        return JIM_OK;
    }
    strLen = Jim_Utf8Length(interp, argv[1]);


    if (argc == 2) {
        splitChars = " \n\t\r";
        splitLen = 4;
    }
    else {
        splitChars = Jim_String(argv[2]);
        splitLen = Jim_Utf8Length(interp, argv[2]);
    }

    noMatchStart = str;
    resObjPtr = Jim_NewListObj(interp, NULL, 0);


    if (splitLen) {
        Jim_Obj *objPtr;
        while (strLen--) {
            const char *sc = splitChars;
            int scLen = splitLen;
            int sl = utf8_tounicode(str, &c);
            while (scLen--) {
                int pc;
                sc += utf8_tounicode(sc, &pc);
                if (c == pc) {
                    objPtr = Jim_NewStringObj(interp, noMatchStart, (str - noMatchStart));
                    Jim_ListAppendElement(interp, resObjPtr, objPtr);
                    noMatchStart = str + sl;
                    break;
                }
            }
            str += sl;
        }
        objPtr = Jim_NewStringObj(interp, noMatchStart, (str - noMatchStart));
        Jim_ListAppendElement(interp, resObjPtr, objPtr);
    }
    else {
        Jim_Obj **commonObj = NULL;
#define NUM_COMMON (128 - 9)
        while (strLen--) {
            int n = utf8_tounicode(str, &c);
#ifdef JIM_OPTIMIZATION
            if (c >= 9 && c < 128) {

                c -= 9;
                if (!commonObj) {
                    commonObj = Jim_Alloc(sizeof(*commonObj) * NUM_COMMON);
                    memset(commonObj, 0, sizeof(*commonObj) * NUM_COMMON);
                }
                if (!commonObj[c]) {
                    commonObj[c] = Jim_NewStringObj(interp, str, 1);
                }
                Jim_ListAppendElement(interp, resObjPtr, commonObj[c]);
                str++;
                continue;
            }
#endif
            Jim_ListAppendElement(interp, resObjPtr, Jim_NewStringObjUtf8(interp, str, 1));
            str += n;
        }
        Jim_Free(commonObj);
    }

    Jim_SetResult(interp, resObjPtr);
    return JIM_OK;
}


static int Jim_JoinCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *joinStr;
    int joinStrLen;

    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "list ?joinString?");
        return JIM_ERR;
    }

    if (argc == 2) {
        joinStr = " ";
        joinStrLen = 1;
    }
    else {
        joinStr = Jim_GetString(argv[2], &joinStrLen);
    }
    Jim_SetResult(interp, Jim_ListJoin(interp, argv[1], joinStr, joinStrLen));
    return JIM_OK;
}


static int Jim_FormatCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "formatString ?arg arg ...?");
        return JIM_ERR;
    }
    objPtr = Jim_FormatString(interp, argv[1], argc - 2, argv + 2);
    if (objPtr == NULL)
        return JIM_ERR;
    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}


static int Jim_ScanCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *listPtr, **outVec;
    int outc, i;

    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "string format ?varName varName ...?");
        return JIM_ERR;
    }
    if (argv[2]->typePtr != &scanFmtStringObjType)
        SetScanFmtFromAny(interp, argv[2]);
    if (FormatGetError(argv[2]) != 0) {
        Jim_SetResultString(interp, FormatGetError(argv[2]), -1);
        return JIM_ERR;
    }
    if (argc > 3) {
        int maxPos = FormatGetMaxPos(argv[2]);
        int count = FormatGetCnvCount(argv[2]);

        if (maxPos > argc - 3) {
            Jim_SetResultString(interp, "\"%n$\" argument index out of range", -1);
            return JIM_ERR;
        }
        else if (count > argc - 3) {
            Jim_SetResultString(interp, "different numbers of variable names and "
                "field specifiers", -1);
            return JIM_ERR;
        }
        else if (count < argc - 3) {
            Jim_SetResultString(interp, "variable is not assigned by any "
                "conversion specifiers", -1);
            return JIM_ERR;
        }
    }
    listPtr = Jim_ScanString(interp, argv[1], argv[2], JIM_ERRMSG);
    if (listPtr == 0)
        return JIM_ERR;
    if (argc > 3) {
        int rc = JIM_OK;
        int count = 0;

        if (listPtr != 0 && listPtr != (Jim_Obj *)EOF) {
            int len = Jim_ListLength(interp, listPtr);

            if (len != 0) {
                JimListGetElements(interp, listPtr, &outc, &outVec);
                for (i = 0; i < outc; ++i) {
                    if (Jim_Length(outVec[i]) > 0) {
                        ++count;
                        if (Jim_SetVariable(interp, argv[3 + i], outVec[i]) != JIM_OK) {
                            rc = JIM_ERR;
                        }
                    }
                }
            }
            Jim_FreeNewObj(interp, listPtr);
        }
        else {
            count = -1;
        }
        if (rc == JIM_OK) {
            Jim_SetResultInt(interp, count);
        }
        return rc;
    }
    else {
        if (listPtr == (Jim_Obj *)EOF) {
            Jim_SetResult(interp, Jim_NewListObj(interp, 0, 0));
            return JIM_OK;
        }
        Jim_SetResult(interp, listPtr);
    }
    return JIM_OK;
}


static int Jim_ErrorCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "message ?stacktrace?");
        return JIM_ERR;
    }
    Jim_SetResult(interp, argv[1]);
    if (argc == 3) {
        JimSetStackTrace(interp, argv[2]);
        return JIM_ERR;
    }
    interp->addStackTrace++;
    return JIM_ERR;
}


static int Jim_LrangeCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;

    if (argc != 4) {
        Jim_WrongNumArgs(interp, 1, argv, "list first last");
        return JIM_ERR;
    }
    if ((objPtr = Jim_ListRange(interp, argv[1], argv[2], argv[3])) == NULL)
        return JIM_ERR;
    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}


static int Jim_LrepeatCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;
    long count;

    if (argc < 2 || Jim_GetLong(interp, argv[1], &count) != JIM_OK || count < 0) {
        Jim_WrongNumArgs(interp, 1, argv, "count ?value ...?");
        return JIM_ERR;
    }

    if (count == 0 || argc == 2) {
        return JIM_OK;
    }

    argc -= 2;
    argv += 2;

    objPtr = Jim_NewListObj(interp, argv, argc);
    while (--count) {
        ListInsertElements(objPtr, -1, argc, argv);
    }

    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}

char **Jim_GetEnviron(void)
{
#if defined(HAVE__NSGETENVIRON)
    return *_NSGetEnviron();
#else
    #if !defined(NO_ENVIRON_EXTERN)
    extern char **environ;
    #endif

    return environ;
#endif
}

void Jim_SetEnviron(char **env)
{
#if defined(HAVE__NSGETENVIRON)
    *_NSGetEnviron() = env;
#else
    #if !defined(NO_ENVIRON_EXTERN)
    extern char **environ;
    #endif

    environ = env;
#endif
}


static int Jim_EnvCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *key;
    const char *val;

    if (argc == 1) {
        char **e = Jim_GetEnviron();

        int i;
        Jim_Obj *listObjPtr = Jim_NewListObj(interp, NULL, 0);

        for (i = 0; e[i]; i++) {
            const char *equals = strchr(e[i], '=');

            if (equals) {
                Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, e[i],
                        equals - e[i]));
                Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, equals + 1, -1));
            }
        }

        Jim_SetResult(interp, listObjPtr);
        return JIM_OK;
    }

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "varName ?default?");
        return JIM_ERR;
    }
    key = Jim_String(argv[1]);
    val = getenv(key);
    if (val == NULL) {
        if (argc < 3) {
            Jim_SetResultFormatted(interp, "environment variable \"%#s\" does not exist", argv[1]);
            return JIM_ERR;
        }
        val = Jim_String(argv[2]);
    }
    Jim_SetResult(interp, Jim_NewStringObj(interp, val, -1));
    return JIM_OK;
}


static int Jim_SourceCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int retval;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "fileName");
        return JIM_ERR;
    }
    retval = Jim_EvalFile(interp, Jim_String(argv[1]));
    if (retval == JIM_RETURN)
        return JIM_OK;
    return retval;
}


static int Jim_LreverseCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *revObjPtr, **ele;
    int len;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "list");
        return JIM_ERR;
    }
    JimListGetElements(interp, argv[1], &len, &ele);
    len--;
    revObjPtr = Jim_NewListObj(interp, NULL, 0);
    while (len >= 0)
        ListAppendElement(revObjPtr, ele[len--]);
    Jim_SetResult(interp, revObjPtr);
    return JIM_OK;
}

static int JimRangeLen(jim_wide start, jim_wide end, jim_wide step)
{
    jim_wide len;

    if (step == 0)
        return -1;
    if (start == end)
        return 0;
    else if (step > 0 && start > end)
        return -1;
    else if (step < 0 && end > start)
        return -1;
    len = end - start;
    if (len < 0)
        len = -len;
    if (step < 0)
        step = -step;
    len = 1 + ((len - 1) / step);
    if (len > INT_MAX)
        len = INT_MAX;
    return (int)((len < 0) ? -1 : len);
}


static int Jim_RangeCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_wide start = 0, end, step = 1;
    int len, i;
    Jim_Obj *objPtr;

    if (argc < 2 || argc > 4) {
        Jim_WrongNumArgs(interp, 1, argv, "?start? end ?step?");
        return JIM_ERR;
    }
    if (argc == 2) {
        if (Jim_GetWide(interp, argv[1], &end) != JIM_OK)
            return JIM_ERR;
    }
    else {
        if (Jim_GetWide(interp, argv[1], &start) != JIM_OK ||
            Jim_GetWide(interp, argv[2], &end) != JIM_OK)
            return JIM_ERR;
        if (argc == 4 && Jim_GetWide(interp, argv[3], &step) != JIM_OK)
            return JIM_ERR;
    }
    if ((len = JimRangeLen(start, end, step)) == -1) {
        Jim_SetResultString(interp, "Invalid (infinite?) range specified", -1);
        return JIM_ERR;
    }
    objPtr = Jim_NewListObj(interp, NULL, 0);
    for (i = 0; i < len; i++)
        ListAppendElement(objPtr, Jim_NewIntObj(interp, start + i * step));
    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}


static int Jim_RandCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_wide min = 0, max = 0, len, maxMul;

    if (argc < 1 || argc > 3) {
        Jim_WrongNumArgs(interp, 1, argv, "?min? max");
        return JIM_ERR;
    }
    if (argc == 1) {
        max = JIM_WIDE_MAX;
    } else if (argc == 2) {
        if (Jim_GetWide(interp, argv[1], &max) != JIM_OK)
            return JIM_ERR;
    } else if (argc == 3) {
        if (Jim_GetWide(interp, argv[1], &min) != JIM_OK ||
            Jim_GetWide(interp, argv[2], &max) != JIM_OK)
            return JIM_ERR;
    }
    len = max-min;
    if (len < 0) {
        Jim_SetResultString(interp, "Invalid arguments (max < min)", -1);
        return JIM_ERR;
    }
    maxMul = JIM_WIDE_MAX - (len ? (JIM_WIDE_MAX%len) : 0);
    while (1) {
        jim_wide r;

        JimRandomBytes(interp, &r, sizeof(jim_wide));
        if (r < 0 || r >= maxMul) continue;
        r = (len == 0) ? 0 : r%len;
        Jim_SetResultInt(interp, min+r);
        return JIM_OK;
    }
}

static const struct {
    const char *name;
    Jim_CmdProc *cmdProc;
} Jim_CoreCommandsTable[] = {
    {"alias", Jim_AliasCoreCommand},
    {"set", Jim_SetCoreCommand},
    {"unset", Jim_UnsetCoreCommand},
    {"puts", Jim_PutsCoreCommand},
    {"+", Jim_AddCoreCommand},
    {"*", Jim_MulCoreCommand},
    {"-", Jim_SubCoreCommand},
    {"/", Jim_DivCoreCommand},
    {"incr", Jim_IncrCoreCommand},
    {"while", Jim_WhileCoreCommand},
    {"loop", Jim_LoopCoreCommand},
    {"for", Jim_ForCoreCommand},
    {"foreach", Jim_ForeachCoreCommand},
    {"lmap", Jim_LmapCoreCommand},
    {"lassign", Jim_LassignCoreCommand},
    {"if", Jim_IfCoreCommand},
    {"switch", Jim_SwitchCoreCommand},
    {"list", Jim_ListCoreCommand},
    {"lindex", Jim_LindexCoreCommand},
    {"lset", Jim_LsetCoreCommand},
    {"lsearch", Jim_LsearchCoreCommand},
    {"llength", Jim_LlengthCoreCommand},
    {"lappend", Jim_LappendCoreCommand},
    {"linsert", Jim_LinsertCoreCommand},
    {"lreplace", Jim_LreplaceCoreCommand},
    {"lsort", Jim_LsortCoreCommand},
    {"append", Jim_AppendCoreCommand},
    {"debug", Jim_DebugCoreCommand},
    {"eval", Jim_EvalCoreCommand},
    {"uplevel", Jim_UplevelCoreCommand},
    {"expr", Jim_ExprCoreCommand},
    {"break", Jim_BreakCoreCommand},
    {"continue", Jim_ContinueCoreCommand},
    {"proc", Jim_ProcCoreCommand},
    {"concat", Jim_ConcatCoreCommand},
    {"return", Jim_ReturnCoreCommand},
    {"upvar", Jim_UpvarCoreCommand},
    {"global", Jim_GlobalCoreCommand},
    {"string", Jim_StringCoreCommand},
    {"time", Jim_TimeCoreCommand},
    {"exit", Jim_ExitCoreCommand},
    {"catch", Jim_CatchCoreCommand},
#ifdef JIM_REFERENCES
    {"ref", Jim_RefCoreCommand},
    {"getref", Jim_GetrefCoreCommand},
    {"setref", Jim_SetrefCoreCommand},
    {"finalize", Jim_FinalizeCoreCommand},
    {"collect", Jim_CollectCoreCommand},
#endif
    {"rename", Jim_RenameCoreCommand},
    {"dict", Jim_DictCoreCommand},
    {"subst", Jim_SubstCoreCommand},
    {"info", Jim_InfoCoreCommand},
    {"exists", Jim_ExistsCoreCommand},
    {"split", Jim_SplitCoreCommand},
    {"join", Jim_JoinCoreCommand},
    {"format", Jim_FormatCoreCommand},
    {"scan", Jim_ScanCoreCommand},
    {"error", Jim_ErrorCoreCommand},
    {"lrange", Jim_LrangeCoreCommand},
    {"lrepeat", Jim_LrepeatCoreCommand},
    {"env", Jim_EnvCoreCommand},
    {"source", Jim_SourceCoreCommand},
    {"lreverse", Jim_LreverseCoreCommand},
    {"range", Jim_RangeCoreCommand},
    {"rand", Jim_RandCoreCommand},
    {"tailcall", Jim_TailcallCoreCommand},
    {"local", Jim_LocalCoreCommand},
    {"upcall", Jim_UpcallCoreCommand},
    {"apply", Jim_ApplyCoreCommand},
    {NULL, NULL},
};

void Jim_RegisterCoreCommands(Jim_Interp *interp)
{
    int i = 0;

    while (Jim_CoreCommandsTable[i].name != NULL) {
        Jim_CreateCommand(interp,
            Jim_CoreCommandsTable[i].name, Jim_CoreCommandsTable[i].cmdProc, NULL, NULL);
        i++;
    }
}

void Jim_MakeErrorMessage(Jim_Interp *interp)
{
    Jim_Obj *argv[2];

    argv[0] = Jim_NewStringObj(interp, "errorInfo", -1);
    argv[1] = interp->result;

    Jim_EvalObjVector(interp, 2, argv);
}

static char **JimSortStringTable(const char *const *tablePtr)
{
    int count;
    char **tablePtrSorted;


    for (count = 0; tablePtr[count]; count++) {
    }


    tablePtrSorted = Jim_Alloc(sizeof(char *) * (count + 1));
    memcpy(tablePtrSorted, tablePtr, sizeof(char *) * count);
    qsort(tablePtrSorted, count, sizeof(char *), qsortCompareStringPointers);
    tablePtrSorted[count] = NULL;

    return tablePtrSorted;
}

static void JimSetFailedEnumResult(Jim_Interp *interp, const char *arg, const char *badtype,
    const char *prefix, const char *const *tablePtr, const char *name)
{
    char **tablePtrSorted;
    int i;

    if (name == NULL) {
        name = "option";
    }

    Jim_SetResultFormatted(interp, "%s%s \"%s\": must be ", badtype, name, arg);
    tablePtrSorted = JimSortStringTable(tablePtr);
    for (i = 0; tablePtrSorted[i]; i++) {
        if (tablePtrSorted[i + 1] == NULL && i > 0) {
            Jim_AppendString(interp, Jim_GetResult(interp), "or ", -1);
        }
        Jim_AppendStrings(interp, Jim_GetResult(interp), prefix, tablePtrSorted[i], NULL);
        if (tablePtrSorted[i + 1]) {
            Jim_AppendString(interp, Jim_GetResult(interp), ", ", -1);
        }
    }
    Jim_Free(tablePtrSorted);
}


int Jim_CheckShowCommands(Jim_Interp *interp, Jim_Obj *objPtr, const char *const *tablePtr)
{
    if (Jim_CompareStringImmediate(interp, objPtr, "-commands")) {
        int i;
        char **tablePtrSorted = JimSortStringTable(tablePtr);
        Jim_SetResult(interp, Jim_NewListObj(interp, NULL, 0));
        for (i = 0; tablePtrSorted[i]; i++) {
            Jim_ListAppendElement(interp, Jim_GetResult(interp), Jim_NewStringObj(interp, tablePtrSorted[i], -1));
        }
        Jim_Free(tablePtrSorted);
        return JIM_OK;
    }
    return JIM_ERR;
}

static const Jim_ObjType getEnumObjType = {
    "get-enum",
    NULL,
    NULL,
    NULL,
    JIM_TYPE_REFERENCES
};

int Jim_GetEnum(Jim_Interp *interp, Jim_Obj *objPtr,
    const char *const *tablePtr, int *indexPtr, const char *name, int flags)
{
    const char *bad = "bad ";
    const char *const *entryPtr = NULL;
    int i;
    int match = -1;
    int arglen;
    const char *arg;

    if (objPtr->typePtr == &getEnumObjType) {
        if (objPtr->internalRep.ptrIntValue.ptr == tablePtr && objPtr->internalRep.ptrIntValue.int1 == flags) {
            *indexPtr = objPtr->internalRep.ptrIntValue.int2;
            return JIM_OK;
        }
    }

    arg = Jim_GetString(objPtr, &arglen);

    *indexPtr = -1;

    for (entryPtr = tablePtr, i = 0; *entryPtr != NULL; entryPtr++, i++) {
        if (Jim_CompareStringImmediate(interp, objPtr, *entryPtr)) {

            match = i;
            goto found;
        }
        if (flags & JIM_ENUM_ABBREV) {
            if (strncmp(arg, *entryPtr, arglen) == 0) {
                if (*arg == '-' && arglen == 1) {
                    break;
                }
                if (match >= 0) {
                    bad = "ambiguous ";
                    goto ambiguous;
                }
                match = i;
            }
        }
    }


    if (match >= 0) {
  found:

        Jim_FreeIntRep(interp, objPtr);
        objPtr->typePtr = &getEnumObjType;
        objPtr->internalRep.ptrIntValue.ptr = (void *)tablePtr;
        objPtr->internalRep.ptrIntValue.int1 = flags;
        objPtr->internalRep.ptrIntValue.int2 = match;

        *indexPtr = match;
        return JIM_OK;
    }

  ambiguous:
    if (flags & JIM_ERRMSG) {
        JimSetFailedEnumResult(interp, arg, bad, "", tablePtr, name);
    }
    return JIM_ERR;
}

int Jim_FindByName(const char *name, const char * const array[], size_t len)
{
    int i;

    for (i = 0; i < (int)len; i++) {
        if (array[i] && strcmp(array[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

int Jim_IsDict(Jim_Obj *objPtr)
{
    return objPtr->typePtr == &dictObjType;
}

int Jim_IsList(Jim_Obj *objPtr)
{
    return objPtr->typePtr == &listObjType;
}

void Jim_SetResultFormatted(Jim_Interp *interp, const char *format, ...)
{

    int len = strlen(format);
    int extra = 0;
    int n = 0;
    const char *params[5];
    int nobjparam = 0;
    Jim_Obj *objparam[5];
    char *buf;
    va_list args;
    int i;

    va_start(args, format);

    for (i = 0; i < len && n < 5; i++) {
        int l;

        if (strncmp(format + i, "%s", 2) == 0) {
            params[n] = va_arg(args, char *);

            l = strlen(params[n]);
        }
        else if (strncmp(format + i, "%#s", 3) == 0) {
            Jim_Obj *objPtr = va_arg(args, Jim_Obj *);

            params[n] = Jim_GetString(objPtr, &l);
            objparam[nobjparam++] = objPtr;
            Jim_IncrRefCount(objPtr);
        }
        else {
            if (format[i] == '%') {
                i++;
            }
            continue;
        }
        n++;
        extra += l;
    }

    len += extra;
    buf = Jim_Alloc(len + 1);
    len = snprintf(buf, len + 1, format, params[0], params[1], params[2], params[3], params[4]);

    va_end(args);

    Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, buf, len));

    for (i = 0; i < nobjparam; i++) {
        Jim_DecrRefCount(interp, objparam[i]);
    }
}


#ifndef jim_ext_package
int Jim_PackageProvide(Jim_Interp *interp, const char *name, const char *ver, int flags)
{
    return JIM_OK;
}
#endif
#ifndef jim_ext_aio
FILE *Jim_AioFilehandle(Jim_Interp *interp, Jim_Obj *fhObj)
{
    Jim_SetResultString(interp, "aio not enabled", -1);
    return NULL;
}
#endif


#include <stdio.h>
#include <string.h>


static int subcmd_null(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{

    return JIM_OK;
}

static const jim_subcmd_type dummy_subcmd = {
    "dummy", NULL, subcmd_null, 0, 0, JIM_MODFLAG_HIDDEN
};

static void add_commands(Jim_Interp *interp, const jim_subcmd_type * ct, const char *sep)
{
    const char *s = "";

    for (; ct->cmd; ct++) {
        if (!(ct->flags & JIM_MODFLAG_HIDDEN)) {
            Jim_AppendStrings(interp, Jim_GetResult(interp), s, ct->cmd, NULL);
            s = sep;
        }
    }
}

static void bad_subcmd(Jim_Interp *interp, const jim_subcmd_type * command_table, const char *type,
    Jim_Obj *cmd, Jim_Obj *subcmd)
{
    Jim_SetResultFormatted(interp, "%#s, %s command \"%#s\": should be ", cmd, type, subcmd);
    add_commands(interp, command_table, ", ");
}

static void show_cmd_usage(Jim_Interp *interp, const jim_subcmd_type * command_table, int argc,
    Jim_Obj *const *argv)
{
    Jim_SetResultFormatted(interp, "Usage: \"%#s command ... \", where command is one of: ", argv[0]);
    add_commands(interp, command_table, ", ");
}

static void add_cmd_usage(Jim_Interp *interp, const jim_subcmd_type * ct, Jim_Obj *cmd)
{
    if (cmd) {
        Jim_AppendStrings(interp, Jim_GetResult(interp), Jim_String(cmd), " ", NULL);
    }
    Jim_AppendStrings(interp, Jim_GetResult(interp), ct->cmd, NULL);
    if (ct->args && *ct->args) {
        Jim_AppendStrings(interp, Jim_GetResult(interp), " ", ct->args, NULL);
    }
}

static void set_wrong_args(Jim_Interp *interp, const jim_subcmd_type * command_table, Jim_Obj *subcmd)
{
    Jim_SetResultString(interp, "wrong # args: should be \"", -1);
    add_cmd_usage(interp, command_table, subcmd);
    Jim_AppendStrings(interp, Jim_GetResult(interp), "\"", NULL);
}

static const Jim_ObjType subcmdLookupObjType = {
    "subcmd-lookup",
    NULL,
    NULL,
    NULL,
    JIM_TYPE_REFERENCES
};

const jim_subcmd_type *Jim_ParseSubCmd(Jim_Interp *interp, const jim_subcmd_type * command_table,
    int argc, Jim_Obj *const *argv)
{
    const jim_subcmd_type *ct;
    const jim_subcmd_type *partial = 0;
    int cmdlen;
    Jim_Obj *cmd;
    const char *cmdstr;
    int help = 0;

    if (argc < 2) {
        Jim_SetResultFormatted(interp, "wrong # args: should be \"%#s command ...\"\n"
            "Use \"%#s -help ?command?\" for help", argv[0], argv[0]);
        return 0;
    }

    cmd = argv[1];


    if (cmd->typePtr == &subcmdLookupObjType) {
        if (cmd->internalRep.ptrIntValue.ptr == command_table) {
            ct = command_table + cmd->internalRep.ptrIntValue.int1;
            goto found;
        }
    }


    if (Jim_CompareStringImmediate(interp, cmd, "-help")) {
        if (argc == 2) {

            show_cmd_usage(interp, command_table, argc, argv);
            return &dummy_subcmd;
        }
        help = 1;


        cmd = argv[2];
    }


    if (Jim_CompareStringImmediate(interp, cmd, "-commands")) {

        Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
        add_commands(interp, command_table, " ");
        return &dummy_subcmd;
    }

    cmdstr = Jim_GetString(cmd, &cmdlen);

    for (ct = command_table; ct->cmd; ct++) {
        if (Jim_CompareStringImmediate(interp, cmd, ct->cmd)) {

            break;
        }
        if (strncmp(cmdstr, ct->cmd, cmdlen) == 0) {
            if (partial) {

                if (help) {

                    show_cmd_usage(interp, command_table, argc, argv);
                    return &dummy_subcmd;
                }
                bad_subcmd(interp, command_table, "ambiguous", argv[0], argv[1 + help]);
                return 0;
            }
            partial = ct;
        }
        continue;
    }


    if (partial && !ct->cmd) {
        ct = partial;
    }

    if (!ct->cmd) {

        if (help) {

            show_cmd_usage(interp, command_table, argc, argv);
            return &dummy_subcmd;
        }
        bad_subcmd(interp, command_table, "unknown", argv[0], argv[1 + help]);
        return 0;
    }

    if (help) {
        Jim_SetResultString(interp, "Usage: ", -1);

        add_cmd_usage(interp, ct, argv[0]);
        return &dummy_subcmd;
    }


    Jim_FreeIntRep(interp, cmd);
    cmd->typePtr = &subcmdLookupObjType;
    cmd->internalRep.ptrIntValue.ptr = (void *)command_table;
    cmd->internalRep.ptrIntValue.int1 = ct - command_table;

found:

    if (argc - 2 < ct->minargs || (ct->maxargs >= 0 && argc - 2 > ct->maxargs)) {
        Jim_SetResultString(interp, "wrong # args: should be \"", -1);

        add_cmd_usage(interp, ct, argv[0]);
        Jim_AppendStrings(interp, Jim_GetResult(interp), "\"", NULL);

        return 0;
    }


    return ct;
}

int Jim_CallSubCmd(Jim_Interp *interp, const jim_subcmd_type * ct, int argc, Jim_Obj *const *argv)
{
    int ret = JIM_ERR;

    if (ct) {
        if (ct->flags & JIM_MODFLAG_FULLARGV) {
            ret = ct->function(interp, argc, argv);
        }
        else {
            ret = ct->function(interp, argc - 2, argv + 2);
        }
        if (ret < 0) {
            set_wrong_args(interp, ct, argv[0]);
            ret = JIM_ERR;
        }
    }
    return ret;
}

int Jim_SubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const jim_subcmd_type *ct =
        Jim_ParseSubCmd(interp, (const jim_subcmd_type *)Jim_CmdPrivData(interp), argc, argv);

    return Jim_CallSubCmd(interp, ct, argc, argv);
}

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>


int utf8_fromunicode(char *p, unsigned uc)
{
    if (uc <= 0x7f) {
        *p = uc;
        return 1;
    }
    else if (uc <= 0x7ff) {
        *p++ = 0xc0 | ((uc & 0x7c0) >> 6);
        *p = 0x80 | (uc & 0x3f);
        return 2;
    }
    else if (uc <= 0xffff) {
        *p++ = 0xe0 | ((uc & 0xf000) >> 12);
        *p++ = 0x80 | ((uc & 0xfc0) >> 6);
        *p = 0x80 | (uc & 0x3f);
        return 3;
    }

    else {
        *p++ = 0xf0 | ((uc & 0x1c0000) >> 18);
        *p++ = 0x80 | ((uc & 0x3f000) >> 12);
        *p++ = 0x80 | ((uc & 0xfc0) >> 6);
        *p = 0x80 | (uc & 0x3f);
        return 4;
    }
}

#include <ctype.h>
#include <string.h>


#define JIM_INTEGER_SPACE 24
#define MAX_FLOAT_WIDTH 320

Jim_Obj *Jim_FormatString(Jim_Interp *interp, Jim_Obj *fmtObjPtr, int objc, Jim_Obj *const *objv)
{
    const char *span, *format, *formatEnd, *msg;
    int numBytes = 0, objIndex = 0, gotXpg = 0, gotSequential = 0;
    static const char * const mixedXPG =
            "cannot mix \"%\" and \"%n$\" conversion specifiers";
    static const char * const badIndex[2] = {
        "not enough arguments for all format specifiers",
        "\"%n$\" argument index out of range"
    };
    int formatLen;
    Jim_Obj *resultPtr;

    char *num_buffer = NULL;
    int num_buffer_size = 0;

    span = format = Jim_GetString(fmtObjPtr, &formatLen);
    formatEnd = format + formatLen;
    resultPtr = Jim_NewEmptyStringObj(interp);

    while (format != formatEnd) {
        char *end;
        int gotMinus, sawFlag;
        int gotPrecision, useShort;
        long width, precision;
        int newXpg;
        int ch;
        int step;
        int doubleType;
        char pad = ' ';
        char spec[2*JIM_INTEGER_SPACE + 12];
        char *p;

        int formatted_chars;
        int formatted_bytes;
        const char *formatted_buf;

        step = utf8_tounicode(format, &ch);
        format += step;
        if (ch != '%') {
            numBytes += step;
            continue;
        }
        if (numBytes) {
            Jim_AppendString(interp, resultPtr, span, numBytes);
            numBytes = 0;
        }


        step = utf8_tounicode(format, &ch);
        if (ch == '%') {
            span = format;
            numBytes = step;
            format += step;
            continue;
        }


        newXpg = 0;
        if (isdigit(ch)) {
            int position = strtoul(format, &end, 10);
            if (*end == '$') {
                newXpg = 1;
                objIndex = position - 1;
                format = end + 1;
                step = utf8_tounicode(format, &ch);
            }
        }
        if (newXpg) {
            if (gotSequential) {
                msg = mixedXPG;
                goto errorMsg;
            }
            gotXpg = 1;
        } else {
            if (gotXpg) {
                msg = mixedXPG;
                goto errorMsg;
            }
            gotSequential = 1;
        }
        if ((objIndex < 0) || (objIndex >= objc)) {
            msg = badIndex[gotXpg];
            goto errorMsg;
        }

        p = spec;
        *p++ = '%';

        gotMinus = 0;
        sawFlag = 1;
        do {
            switch (ch) {
            case '-':
                gotMinus = 1;
                break;
            case '0':
                pad = ch;
                break;
            case ' ':
            case '+':
            case '#':
                break;
            default:
                sawFlag = 0;
                continue;
            }
            *p++ = ch;
            format += step;
            step = utf8_tounicode(format, &ch);

        } while (sawFlag && (p - spec <= 5));


        width = 0;
        if (isdigit(ch)) {
            width = strtoul(format, &end, 10);
            format = end;
            step = utf8_tounicode(format, &ch);
        } else if (ch == '*') {
            if (objIndex >= objc - 1) {
                msg = badIndex[gotXpg];
                goto errorMsg;
            }
            if (Jim_GetLong(interp, objv[objIndex], &width) != JIM_OK) {
                goto error;
            }
            if (width < 0) {
                width = -width;
                if (!gotMinus) {
                    *p++ = '-';
                    gotMinus = 1;
                }
            }
            objIndex++;
            format += step;
            step = utf8_tounicode(format, &ch);
        }


        gotPrecision = precision = 0;
        if (ch == '.') {
            gotPrecision = 1;
            format += step;
            step = utf8_tounicode(format, &ch);
        }
        if (isdigit(ch)) {
            precision = strtoul(format, &end, 10);
            format = end;
            step = utf8_tounicode(format, &ch);
        } else if (ch == '*') {
            if (objIndex >= objc - 1) {
                msg = badIndex[gotXpg];
                goto errorMsg;
            }
            if (Jim_GetLong(interp, objv[objIndex], &precision) != JIM_OK) {
                goto error;
            }


            if (precision < 0) {
                precision = 0;
            }
            objIndex++;
            format += step;
            step = utf8_tounicode(format, &ch);
        }


        useShort = 0;
        if (ch == 'h') {
            useShort = 1;
            format += step;
            step = utf8_tounicode(format, &ch);
        } else if (ch == 'l') {

            format += step;
            step = utf8_tounicode(format, &ch);
            if (ch == 'l') {
                format += step;
                step = utf8_tounicode(format, &ch);
            }
        }

        format += step;
        span = format;


        if (ch == 'i') {
            ch = 'd';
        }

        doubleType = 0;

        switch (ch) {
        case '\0':
            msg = "format string ended in middle of field specifier";
            goto errorMsg;
        case 's': {
            formatted_buf = Jim_GetString(objv[objIndex], &formatted_bytes);
            formatted_chars = Jim_Utf8Length(interp, objv[objIndex]);
            if (gotPrecision && (precision < formatted_chars)) {

                formatted_chars = precision;
                formatted_bytes = utf8_index(formatted_buf, precision);
            }
            break;
        }
        case 'c': {
            jim_wide code;

            if (Jim_GetWide(interp, objv[objIndex], &code) != JIM_OK) {
                goto error;
            }

            formatted_bytes = utf8_getchars(spec, code);
            formatted_buf = spec;
            formatted_chars = 1;
            break;
        }
        case 'b': {
                unsigned jim_wide w;
                int length;
                int i;
                int j;

                if (Jim_GetWide(interp, objv[objIndex], (jim_wide *)&w) != JIM_OK) {
                    goto error;
                }
                length = sizeof(w) * 8;



                if (num_buffer_size < length + 1) {
                    num_buffer_size = length + 1;
                    num_buffer = Jim_Realloc(num_buffer, num_buffer_size);
                }

                j = 0;
                for (i = length; i > 0; ) {
                        i--;
                    if (w & ((unsigned jim_wide)1 << i)) {
                                num_buffer[j++] = '1';
                        }
                        else if (j || i == 0) {
                                num_buffer[j++] = '0';
                        }
                }
                num_buffer[j] = 0;
                formatted_chars = formatted_bytes = j;
                formatted_buf = num_buffer;
                break;
        }

        case 'e':
        case 'E':
        case 'f':
        case 'g':
        case 'G':
            doubleType = 1;

        case 'd':
        case 'u':
        case 'o':
        case 'x':
        case 'X': {
            jim_wide w;
            double d;
            int length;


            if (width) {
                p += sprintf(p, "%ld", width);
            }
            if (gotPrecision) {
                p += sprintf(p, ".%ld", precision);
            }


            if (doubleType) {
                if (Jim_GetDouble(interp, objv[objIndex], &d) != JIM_OK) {
                    goto error;
                }
                length = MAX_FLOAT_WIDTH;
            }
            else {
                if (Jim_GetWide(interp, objv[objIndex], &w) != JIM_OK) {
                    goto error;
                }
                length = JIM_INTEGER_SPACE;
                if (useShort) {
                    if (ch == 'd') {
                        w = (short)w;
                    }
                    else {
                        w = (unsigned short)w;
                    }
                }
                *p++ = 'l';
#ifdef HAVE_LONG_LONG
                if (sizeof(long long) == sizeof(jim_wide)) {
                    *p++ = 'l';
                }
#endif
            }

            *p++ = (char) ch;
            *p = '\0';


            if (width > 10000 || length > 10000 || precision > 10000) {
                Jim_SetResultString(interp, "format too long", -1);
                goto error;
            }



            if (width > length) {
                length = width;
            }
            if (gotPrecision) {
                length += precision;
            }


            if (num_buffer_size < length + 1) {
                num_buffer_size = length + 1;
                num_buffer = Jim_Realloc(num_buffer, num_buffer_size);
            }

            if (doubleType) {
                snprintf(num_buffer, length + 1, spec, d);
            }
            else {
                formatted_bytes = snprintf(num_buffer, length + 1, spec, w);
            }
            formatted_chars = formatted_bytes = strlen(num_buffer);
            formatted_buf = num_buffer;
            break;
        }

        default: {

            spec[0] = ch;
            spec[1] = '\0';
            Jim_SetResultFormatted(interp, "bad field specifier \"%s\"", spec);
            goto error;
        }
        }

        if (!gotMinus) {
            while (formatted_chars < width) {
                Jim_AppendString(interp, resultPtr, &pad, 1);
                formatted_chars++;
            }
        }

        Jim_AppendString(interp, resultPtr, formatted_buf, formatted_bytes);

        while (formatted_chars < width) {
            Jim_AppendString(interp, resultPtr, &pad, 1);
            formatted_chars++;
        }

        objIndex += gotSequential;
    }
    if (numBytes) {
        Jim_AppendString(interp, resultPtr, span, numBytes);
    }

    Jim_Free(num_buffer);
    return resultPtr;

  errorMsg:
    Jim_SetResultString(interp, msg, -1);
  error:
    Jim_FreeNewObj(interp, resultPtr);
    Jim_Free(num_buffer);
    return NULL;
}


#if defined(JIM_REGEXP)
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>



#define REG_MAX_PAREN 100



#define	END	0
#define	BOL	1
#define	EOL	2
#define	ANY	3
#define	ANYOF	4
#define	ANYBUT	5
#define	BRANCH	6
#define	BACK	7
#define	EXACTLY	8
#define	NOTHING	9
#define	REP	10
#define	REPMIN	11
#define	REPX	12
#define	REPXMIN	13
#define	BOLX	14
#define	EOLX	15
#define	WORDA	16
#define	WORDZ	17

#define	OPENNC 	1000
#define	OPEN   	1001




#define	CLOSENC	2000
#define	CLOSE	2001
#define	CLOSE_END	(CLOSE+REG_MAX_PAREN)

#define	REG_MAGIC	0xFADED00D


#define	OP(preg, p)	(preg->program[p])
#define	NEXT(preg, p)	(preg->program[p + 1])
#define	OPERAND(p)	((p) + 2)




#define	FAIL(R,M)	{ (R)->err = (M); return (M); }
#define	ISMULT(c)	((c) == '*' || (c) == '+' || (c) == '?' || (c) == '{')
#define	META		"^$.[()|?{+*"

#define	HASWIDTH	1
#define	SIMPLE		2
#define	SPSTART		4
#define	WORST		0

#define MAX_REP_COUNT 1000000

static int reg(regex_t *preg, int paren, int *flagp );
static int regpiece(regex_t *preg, int *flagp );
static int regbranch(regex_t *preg, int *flagp );
static int regatom(regex_t *preg, int *flagp );
static int regnode(regex_t *preg, int op );
static int regnext(regex_t *preg, int p );
static void regc(regex_t *preg, int b );
static int reginsert(regex_t *preg, int op, int size, int opnd );
static void regtail(regex_t *preg, int p, int val);
static void regoptail(regex_t *preg, int p, int val );
static int regopsize(regex_t *preg, int p );

static int reg_range_find(const int *string, int c);
static const char *str_find(const char *string, int c, int nocase);
static int prefix_cmp(const int *prog, int proglen, const char *string, int nocase);


#ifdef DEBUG
static int regnarrate = 0;
static void regdump(regex_t *preg);
static const char *regprop( int op );
#endif


static int str_int_len(const int *seq)
{
	int n = 0;
	while (*seq++) {
		n++;
	}
	return n;
}

int regcomp(regex_t *preg, const char *exp, int cflags)
{
	int scan;
	int longest;
	unsigned len;
	int flags;

#ifdef DEBUG
	fprintf(stderr, "Compiling: '%s'\n", exp);
#endif
	memset(preg, 0, sizeof(*preg));

	if (exp == NULL)
		FAIL(preg, REG_ERR_NULL_ARGUMENT);


	preg->cflags = cflags;
	preg->regparse = exp;


	preg->proglen = (strlen(exp) + 1) * 5;
	preg->program = malloc(preg->proglen * sizeof(int));
	if (preg->program == NULL)
		FAIL(preg, REG_ERR_NOMEM);

	regc(preg, REG_MAGIC);
	if (reg(preg, 0, &flags) == 0) {
		return preg->err;
	}


	if (preg->re_nsub >= REG_MAX_PAREN)
		FAIL(preg,REG_ERR_TOO_BIG);


	preg->regstart = 0;
	preg->reganch = 0;
	preg->regmust = 0;
	preg->regmlen = 0;
	scan = 1;
	if (OP(preg, regnext(preg, scan)) == END) {
		scan = OPERAND(scan);


		if (OP(preg, scan) == EXACTLY) {
			preg->regstart = preg->program[OPERAND(scan)];
		}
		else if (OP(preg, scan) == BOL)
			preg->reganch++;

		if (flags&SPSTART) {
			longest = 0;
			len = 0;
			for (; scan != 0; scan = regnext(preg, scan)) {
				if (OP(preg, scan) == EXACTLY) {
					int plen = str_int_len(preg->program + OPERAND(scan));
					if (plen >= len) {
						longest = OPERAND(scan);
						len = plen;
					}
				}
			}
			preg->regmust = longest;
			preg->regmlen = len;
		}
	}

#ifdef DEBUG
	regdump(preg);
#endif

	return 0;
}

static int reg(regex_t *preg, int paren, int *flagp )
{
	int ret;
	int br;
	int ender;
	int parno = 0;
	int flags;

	*flagp = HASWIDTH;


	if (paren) {
		if (preg->regparse[0] == '?' && preg->regparse[1] == ':') {

			preg->regparse += 2;
			parno = -1;
		}
		else {
			parno = ++preg->re_nsub;
		}
		ret = regnode(preg, OPEN+parno);
	} else
		ret = 0;


	br = regbranch(preg, &flags);
	if (br == 0)
		return 0;
	if (ret != 0)
		regtail(preg, ret, br);
	else
		ret = br;
	if (!(flags&HASWIDTH))
		*flagp &= ~HASWIDTH;
	*flagp |= flags&SPSTART;
	while (*preg->regparse == '|') {
		preg->regparse++;
		br = regbranch(preg, &flags);
		if (br == 0)
			return 0;
		regtail(preg, ret, br);
		if (!(flags&HASWIDTH))
			*flagp &= ~HASWIDTH;
		*flagp |= flags&SPSTART;
	}


	ender = regnode(preg, (paren) ? CLOSE+parno : END);
	regtail(preg, ret, ender);


	for (br = ret; br != 0; br = regnext(preg, br))
		regoptail(preg, br, ender);


	if (paren && *preg->regparse++ != ')') {
		preg->err = REG_ERR_UNMATCHED_PAREN;
		return 0;
	} else if (!paren && *preg->regparse != '\0') {
		if (*preg->regparse == ')') {
			preg->err = REG_ERR_UNMATCHED_PAREN;
			return 0;
		} else {
			preg->err = REG_ERR_JUNK_ON_END;
			return 0;
		}
	}

	return(ret);
}

static int regbranch(regex_t *preg, int *flagp )
{
	int ret;
	int chain;
	int latest;
	int flags;

	*flagp = WORST;

	ret = regnode(preg, BRANCH);
	chain = 0;
	while (*preg->regparse != '\0' && *preg->regparse != ')' &&
	       *preg->regparse != '|') {
		latest = regpiece(preg, &flags);
		if (latest == 0)
			return 0;
		*flagp |= flags&HASWIDTH;
		if (chain == 0) {
			*flagp |= flags&SPSTART;
		}
		else {
			regtail(preg, chain, latest);
		}
		chain = latest;
	}
	if (chain == 0)
		(void) regnode(preg, NOTHING);

	return(ret);
}

static int regpiece(regex_t *preg, int *flagp)
{
	int ret;
	char op;
	int next;
	int flags;
	int min;
	int max;

	ret = regatom(preg, &flags);
	if (ret == 0)
		return 0;

	op = *preg->regparse;
	if (!ISMULT(op)) {
		*flagp = flags;
		return(ret);
	}

	if (!(flags&HASWIDTH) && op != '?') {
		preg->err = REG_ERR_OPERAND_COULD_BE_EMPTY;
		return 0;
	}


	if (op == '{') {
		char *end;

		min = strtoul(preg->regparse + 1, &end, 10);
		if (end == preg->regparse + 1) {
			preg->err = REG_ERR_BAD_COUNT;
			return 0;
		}
		if (*end == '}') {
			max = min;
		}
		else if (*end == '\0') {
			preg->err = REG_ERR_UNMATCHED_BRACES;
			return 0;
		}
		else {
			preg->regparse = end;
			max = strtoul(preg->regparse + 1, &end, 10);
			if (*end != '}') {
				preg->err = REG_ERR_UNMATCHED_BRACES;
				return 0;
			}
		}
		if (end == preg->regparse + 1) {
			max = MAX_REP_COUNT;
		}
		else if (max < min || max >= 100) {
			preg->err = REG_ERR_BAD_COUNT;
			return 0;
		}
		if (min >= 100) {
			preg->err = REG_ERR_BAD_COUNT;
			return 0;
		}

		preg->regparse = strchr(preg->regparse, '}');
	}
	else {
		min = (op == '+');
		max = (op == '?' ? 1 : MAX_REP_COUNT);
	}

	if (preg->regparse[1] == '?') {
		preg->regparse++;
		next = reginsert(preg, flags & SIMPLE ? REPMIN : REPXMIN, 5, ret);
	}
	else {
		next = reginsert(preg, flags & SIMPLE ? REP: REPX, 5, ret);
	}
	preg->program[ret + 2] = max;
	preg->program[ret + 3] = min;
	preg->program[ret + 4] = 0;

	*flagp = (min) ? (WORST|HASWIDTH) : (WORST|SPSTART);

	if (!(flags & SIMPLE)) {
		int back = regnode(preg, BACK);
		regtail(preg, back, ret);
		regtail(preg, next, back);
	}

	preg->regparse++;
	if (ISMULT(*preg->regparse)) {
		preg->err = REG_ERR_NESTED_COUNT;
		return 0;
	}

	return ret;
}

static void reg_addrange(regex_t *preg, int lower, int upper)
{
	if (lower > upper) {
		reg_addrange(preg, upper, lower);
	}

	regc(preg, upper - lower + 1);
	regc(preg, lower);
}

static void reg_addrange_str(regex_t *preg, const char *str)
{
	while (*str) {
		reg_addrange(preg, *str, *str);
		str++;
	}
}

static int reg_utf8_tounicode_case(const char *s, int *uc, int upper)
{
	int l = utf8_tounicode(s, uc);
	if (upper) {
		*uc = utf8_upper(*uc);
	}
	return l;
}

static int hexdigitval(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int parse_hex(const char *s, int n, int *uc)
{
	int val = 0;
	int k;

	for (k = 0; k < n; k++) {
		int c = hexdigitval(*s++);
		if (c == -1) {
			break;
		}
		val = (val << 4) | c;
	}
	if (k) {
		*uc = val;
	}
	return k;
}

static int reg_decode_escape(const char *s, int *ch)
{
	int n;
	const char *s0 = s;

	*ch = *s++;

	switch (*ch) {
		case 'b': *ch = '\b'; break;
		case 'e': *ch = 27; break;
		case 'f': *ch = '\f'; break;
		case 'n': *ch = '\n'; break;
		case 'r': *ch = '\r'; break;
		case 't': *ch = '\t'; break;
		case 'v': *ch = '\v'; break;
		case 'u':
			if (*s == '{') {

				n = parse_hex(s + 1, 6, ch);
				if (n > 0 && s[n + 1] == '}' && *ch >= 0 && *ch <= 0x1fffff) {
					s += n + 2;
				}
				else {

					*ch = 'u';
				}
			}
			else if ((n = parse_hex(s, 4, ch)) > 0) {
				s += n;
			}
			break;
		case 'U':
			if ((n = parse_hex(s, 8, ch)) > 0) {
				s += n;
			}
			break;
		case 'x':
			if ((n = parse_hex(s, 2, ch)) > 0) {
				s += n;
			}
			break;
		case '\0':
			s--;
			*ch = '\\';
			break;
	}
	return s - s0;
}

static int regatom(regex_t *preg, int *flagp)
{
	int ret;
	int flags;
	int nocase = (preg->cflags & REG_ICASE);

	int ch;
	int n = reg_utf8_tounicode_case(preg->regparse, &ch, nocase);

	*flagp = WORST;

	preg->regparse += n;
	switch (ch) {

	case '^':
		ret = regnode(preg, BOL);
		break;
	case '$':
		ret = regnode(preg, EOL);
		break;
	case '.':
		ret = regnode(preg, ANY);
		*flagp |= HASWIDTH|SIMPLE;
		break;
	case '[': {
			const char *pattern = preg->regparse;

			if (*pattern == '^') {
				ret = regnode(preg, ANYBUT);
				pattern++;
			} else
				ret = regnode(preg, ANYOF);


			if (*pattern == ']' || *pattern == '-') {
				reg_addrange(preg, *pattern, *pattern);
				pattern++;
			}

			while (*pattern && *pattern != ']') {

				int start;
				int end;

				enum {
					CC_ALPHA, CC_ALNUM, CC_SPACE, CC_BLANK, CC_UPPER, CC_LOWER,
					CC_DIGIT, CC_XDIGIT, CC_CNTRL, CC_GRAPH, CC_PRINT, CC_PUNCT,
					CC_NUM
				};
				int cc;

				pattern += reg_utf8_tounicode_case(pattern, &start, nocase);
				if (start == '\\') {

					switch (*pattern) {
						case 's':
							pattern++;
							cc = CC_SPACE;
							goto cc_switch;
						case 'd':
							pattern++;
							cc = CC_DIGIT;
							goto cc_switch;
						case 'w':
							pattern++;
							reg_addrange(preg, '_', '_');
							cc = CC_ALNUM;
							goto cc_switch;
					}
					pattern += reg_decode_escape(pattern, &start);
					if (start == 0) {
						preg->err = REG_ERR_NULL_CHAR;
						return 0;
					}
				}
				if (pattern[0] == '-' && pattern[1] && pattern[1] != ']') {

					pattern += utf8_tounicode(pattern, &end);
					pattern += reg_utf8_tounicode_case(pattern, &end, nocase);
					if (end == '\\') {
						pattern += reg_decode_escape(pattern, &end);
						if (end == 0) {
							preg->err = REG_ERR_NULL_CHAR;
							return 0;
						}
					}

					reg_addrange(preg, start, end);
					continue;
				}
				if (start == '[' && pattern[0] == ':') {
					static const char *character_class[] = {
						":alpha:", ":alnum:", ":space:", ":blank:", ":upper:", ":lower:",
						":digit:", ":xdigit:", ":cntrl:", ":graph:", ":print:", ":punct:",
					};

					for (cc = 0; cc < CC_NUM; cc++) {
						n = strlen(character_class[cc]);
						if (strncmp(pattern, character_class[cc], n) == 0) {

							pattern += n + 1;
							break;
						}
					}
					if (cc != CC_NUM) {
cc_switch:
						switch (cc) {
							case CC_ALNUM:
								reg_addrange(preg, '0', '9');

							case CC_ALPHA:
								if ((preg->cflags & REG_ICASE) == 0) {
									reg_addrange(preg, 'a', 'z');
								}
								reg_addrange(preg, 'A', 'Z');
								break;
							case CC_SPACE:
								reg_addrange_str(preg, " \t\r\n\f\v");
								break;
							case CC_BLANK:
								reg_addrange_str(preg, " \t");
								break;
							case CC_UPPER:
								reg_addrange(preg, 'A', 'Z');
								break;
							case CC_LOWER:
								reg_addrange(preg, 'a', 'z');
								break;
							case CC_XDIGIT:
								reg_addrange(preg, 'a', 'f');
								reg_addrange(preg, 'A', 'F');

							case CC_DIGIT:
								reg_addrange(preg, '0', '9');
								break;
							case CC_CNTRL:
								reg_addrange(preg, 0, 31);
								reg_addrange(preg, 127, 127);
								break;
							case CC_PRINT:
								reg_addrange(preg, ' ', '~');
								break;
							case CC_GRAPH:
								reg_addrange(preg, '!', '~');
								break;
							case CC_PUNCT:
								reg_addrange(preg, '!', '/');
								reg_addrange(preg, ':', '@');
								reg_addrange(preg, '[', '`');
								reg_addrange(preg, '{', '~');
								break;
						}
						continue;
					}
				}

				reg_addrange(preg, start, start);
			}
			regc(preg, '\0');

			if (*pattern) {
				pattern++;
			}
			preg->regparse = pattern;

			*flagp |= HASWIDTH|SIMPLE;
		}
		break;
	case '(':
		ret = reg(preg, 1, &flags);
		if (ret == 0)
			return 0;
		*flagp |= flags&(HASWIDTH|SPSTART);
		break;
	case '\0':
	case '|':
	case ')':
		preg->err = REG_ERR_INTERNAL;
		return 0;
	case '?':
	case '+':
	case '*':
	case '{':
		preg->err = REG_ERR_COUNT_FOLLOWS_NOTHING;
		return 0;
	case '\\':
		ch = *preg->regparse++;
		switch (ch) {
		case '\0':
			preg->err = REG_ERR_TRAILING_BACKSLASH;
			return 0;
		case 'A':
			ret = regnode(preg, BOLX);
			break;
		case 'Z':
			ret = regnode(preg, EOLX);
			break;
		case '<':
		case 'm':
			ret = regnode(preg, WORDA);
			break;
		case '>':
		case 'M':
			ret = regnode(preg, WORDZ);
			break;
		case 'd':
		case 'D':
			ret = regnode(preg, ch == 'd' ? ANYOF : ANYBUT);
			reg_addrange(preg, '0', '9');
			regc(preg, '\0');
			*flagp |= HASWIDTH|SIMPLE;
			break;
		case 'w':
		case 'W':
			ret = regnode(preg, ch == 'w' ? ANYOF : ANYBUT);
			if ((preg->cflags & REG_ICASE) == 0) {
				reg_addrange(preg, 'a', 'z');
			}
			reg_addrange(preg, 'A', 'Z');
			reg_addrange(preg, '0', '9');
			reg_addrange(preg, '_', '_');
			regc(preg, '\0');
			*flagp |= HASWIDTH|SIMPLE;
			break;
		case 's':
		case 'S':
			ret = regnode(preg, ch == 's' ? ANYOF : ANYBUT);
			reg_addrange_str(preg," \t\r\n\f\v");
			regc(preg, '\0');
			*flagp |= HASWIDTH|SIMPLE;
			break;

		default:


			preg->regparse--;
			goto de_fault;
		}
		break;
	de_fault:
	default: {
			int added = 0;


			preg->regparse -= n;

			ret = regnode(preg, EXACTLY);



			while (*preg->regparse && strchr(META, *preg->regparse) == NULL) {
				n = reg_utf8_tounicode_case(preg->regparse, &ch, (preg->cflags & REG_ICASE));
				if (ch == '\\' && preg->regparse[n]) {
					if (strchr("<>mMwWdDsSAZ", preg->regparse[n])) {

						break;
					}
					n += reg_decode_escape(preg->regparse + n, &ch);
					if (ch == 0) {
						preg->err = REG_ERR_NULL_CHAR;
						return 0;
					}
				}


				if (ISMULT(preg->regparse[n])) {

					if (added) {

						break;
					}

					regc(preg, ch);
					added++;
					preg->regparse += n;
					break;
				}


				regc(preg, ch);
				added++;
				preg->regparse += n;
			}
			regc(preg, '\0');

			*flagp |= HASWIDTH;
			if (added == 1)
				*flagp |= SIMPLE;
			break;
		}
		break;
	}

	return(ret);
}

static void reg_grow(regex_t *preg, int n)
{
	if (preg->p + n >= preg->proglen) {
		preg->proglen = (preg->p + n) * 2;
		preg->program = realloc(preg->program, preg->proglen * sizeof(int));
	}
}


static int regnode(regex_t *preg, int op)
{
	reg_grow(preg, 2);


	preg->program[preg->p++] = op;
	preg->program[preg->p++] = 0;


	return preg->p - 2;
}

static void regc(regex_t *preg, int b )
{
	reg_grow(preg, 1);
	preg->program[preg->p++] = b;
}

static int reginsert(regex_t *preg, int op, int size, int opnd )
{
	reg_grow(preg, size);


	memmove(preg->program + opnd + size, preg->program + opnd, sizeof(int) * (preg->p - opnd));

	memset(preg->program + opnd, 0, sizeof(int) * size);

	preg->program[opnd] = op;

	preg->p += size;

	return opnd + size;
}

static void regtail(regex_t *preg, int p, int val)
{
	int scan;
	int temp;
	int offset;


	scan = p;
	for (;;) {
		temp = regnext(preg, scan);
		if (temp == 0)
			break;
		scan = temp;
	}

	if (OP(preg, scan) == BACK)
		offset = scan - val;
	else
		offset = val - scan;

	preg->program[scan + 1] = offset;
}


static void regoptail(regex_t *preg, int p, int val )
{

	if (p != 0 && OP(preg, p) == BRANCH) {
		regtail(preg, OPERAND(p), val);
	}
}


static int regtry(regex_t *preg, const char *string );
static int regmatch(regex_t *preg, int prog);
static int regrepeat(regex_t *preg, int p, int max);

int regexec(regex_t  *preg,  const  char *string, size_t nmatch, regmatch_t pmatch[], int eflags)
{
	const char *s;
	int scan;


	if (preg == NULL || preg->program == NULL || string == NULL) {
		return REG_ERR_NULL_ARGUMENT;
	}


	if (*preg->program != REG_MAGIC) {
		return REG_ERR_CORRUPTED;
	}

#ifdef DEBUG
	fprintf(stderr, "regexec: %s\n", string);
	regdump(preg);
#endif

	preg->eflags = eflags;
	preg->pmatch = pmatch;
	preg->nmatch = nmatch;
	preg->start = string;


	for (scan = OPERAND(1); scan != 0; scan += regopsize(preg, scan)) {
		int op = OP(preg, scan);
		if (op == END)
			break;
		if (op == REPX || op == REPXMIN)
			preg->program[scan + 4] = 0;
	}


	if (preg->regmust != 0) {
		s = string;
		while ((s = str_find(s, preg->program[preg->regmust], preg->cflags & REG_ICASE)) != NULL) {
			if (prefix_cmp(preg->program + preg->regmust, preg->regmlen, s, preg->cflags & REG_ICASE) >= 0) {
				break;
			}
			s++;
		}
		if (s == NULL)
			return REG_NOMATCH;
	}


	preg->regbol = string;


	if (preg->reganch) {
		if (eflags & REG_NOTBOL) {

			goto nextline;
		}
		while (1) {
			if (regtry(preg, string)) {
				return REG_NOERROR;
			}
			if (*string) {
nextline:
				if (preg->cflags & REG_NEWLINE) {

					string = strchr(string, '\n');
					if (string) {
						preg->regbol = ++string;
						continue;
					}
				}
			}
			return REG_NOMATCH;
		}
	}


	s = string;
	if (preg->regstart != '\0') {

		while ((s = str_find(s, preg->regstart, preg->cflags & REG_ICASE)) != NULL) {
			if (regtry(preg, s))
				return REG_NOERROR;
			s++;
		}
	}
	else

		while (1) {
			if (regtry(preg, s))
				return REG_NOERROR;
			if (*s == '\0') {
				break;
			}
			else {
				int c;
				s += utf8_tounicode(s, &c);
			}
		}


	return REG_NOMATCH;
}


static int regtry( regex_t *preg, const char *string )
{
	int i;

	preg->reginput = string;

	for (i = 0; i < preg->nmatch; i++) {
		preg->pmatch[i].rm_so = -1;
		preg->pmatch[i].rm_eo = -1;
	}
	if (regmatch(preg, 1)) {
		preg->pmatch[0].rm_so = string - preg->start;
		preg->pmatch[0].rm_eo = preg->reginput - preg->start;
		return(1);
	} else
		return(0);
}

static int prefix_cmp(const int *prog, int proglen, const char *string, int nocase)
{
	const char *s = string;
	while (proglen && *s) {
		int ch;
		int n = reg_utf8_tounicode_case(s, &ch, nocase);
		if (ch != *prog) {
			return -1;
		}
		prog++;
		s += n;
		proglen--;
	}
	if (proglen == 0) {
		return s - string;
	}
	return -1;
}

static int reg_range_find(const int *range, int c)
{
	while (*range) {

		if (c >= range[1] && c <= (range[0] + range[1] - 1)) {
			return 1;
		}
		range += 2;
	}
	return 0;
}

static const char *str_find(const char *string, int c, int nocase)
{
	if (nocase) {

		c = utf8_upper(c);
	}
	while (*string) {
		int ch;
		int n = reg_utf8_tounicode_case(string, &ch, nocase);
		if (c == ch) {
			return string;
		}
		string += n;
	}
	return NULL;
}

static int reg_iseol(regex_t *preg, int ch)
{
	if (preg->cflags & REG_NEWLINE) {
		return ch == '\0' || ch == '\n';
	}
	else {
		return ch == '\0';
	}
}

static int regmatchsimplerepeat(regex_t *preg, int scan, int matchmin)
{
	int nextch = '\0';
	const char *save;
	int no;
	int c;

	int max = preg->program[scan + 2];
	int min = preg->program[scan + 3];
	int next = regnext(preg, scan);

	if (OP(preg, next) == EXACTLY) {
		nextch = preg->program[OPERAND(next)];
	}
	save = preg->reginput;
	no = regrepeat(preg, scan + 5, max);
	if (no < min) {
		return 0;
	}
	if (matchmin) {

		max = no;
		no = min;
	}

	while (1) {
		if (matchmin) {
			if (no > max) {
				break;
			}
		}
		else {
			if (no < min) {
				break;
			}
		}
		preg->reginput = save + utf8_index(save, no);
		reg_utf8_tounicode_case(preg->reginput, &c, (preg->cflags & REG_ICASE));

		if (reg_iseol(preg, nextch) || c == nextch) {
			if (regmatch(preg, next)) {
				return(1);
			}
		}
		if (matchmin) {

			no++;
		}
		else {

			no--;
		}
	}
	return(0);
}

static int regmatchrepeat(regex_t *preg, int scan, int matchmin)
{
	int *scanpt = preg->program + scan;

	int max = scanpt[2];
	int min = scanpt[3];


	if (scanpt[4] < min) {

		scanpt[4]++;
		if (regmatch(preg, scan + 5)) {
			return 1;
		}
		scanpt[4]--;
		return 0;
	}
	if (scanpt[4] > max) {
		return 0;
	}

	if (matchmin) {

		if (regmatch(preg, regnext(preg, scan))) {
			return 1;
		}

		scanpt[4]++;
		if (regmatch(preg, scan + 5)) {
			return 1;
		}
		scanpt[4]--;
		return 0;
	}

	if (scanpt[4] < max) {
		scanpt[4]++;
		if (regmatch(preg, scan + 5)) {
			return 1;
		}
		scanpt[4]--;
	}

	return regmatch(preg, regnext(preg, scan));
}


static int regmatch(regex_t *preg, int prog)
{
	int scan;
	int next;
	const char *save;

	scan = prog;

#ifdef DEBUG
	if (scan != 0 && regnarrate)
		fprintf(stderr, "%s(\n", regprop(scan));
#endif
	while (scan != 0) {
		int n;
		int c;
#ifdef DEBUG
		if (regnarrate) {
			fprintf(stderr, "%3d: %s...\n", scan, regprop(OP(preg, scan)));
		}
#endif
		next = regnext(preg, scan);
		n = reg_utf8_tounicode_case(preg->reginput, &c, (preg->cflags & REG_ICASE));

		switch (OP(preg, scan)) {
		case BOLX:
			if ((preg->eflags & REG_NOTBOL)) {
				return(0);
			}

		case BOL:
			if (preg->reginput != preg->regbol) {
				return(0);
			}
			break;
		case EOLX:
			if (c != 0) {

				return 0;
			}
			break;
		case EOL:
			if (!reg_iseol(preg, c)) {
				return(0);
			}
			break;
		case WORDA:

			if ((!isalnum(UCHAR(c))) && c != '_')
				return(0);

			if (preg->reginput > preg->regbol &&
				(isalnum(UCHAR(preg->reginput[-1])) || preg->reginput[-1] == '_'))
				return(0);
			break;
		case WORDZ:

			if (preg->reginput > preg->regbol) {

				if (reg_iseol(preg, c) || !isalnum(UCHAR(c)) || c != '_') {
					c = preg->reginput[-1];

					if (isalnum(UCHAR(c)) || c == '_') {
						break;
					}
				}
			}

			return(0);

		case ANY:
			if (reg_iseol(preg, c))
				return 0;
			preg->reginput += n;
			break;
		case EXACTLY: {
				int opnd;
				int len;
				int slen;

				opnd = OPERAND(scan);
				len = str_int_len(preg->program + opnd);

				slen = prefix_cmp(preg->program + opnd, len, preg->reginput, preg->cflags & REG_ICASE);
				if (slen < 0) {
					return(0);
				}
				preg->reginput += slen;
			}
			break;
		case ANYOF:
			if (reg_iseol(preg, c) || reg_range_find(preg->program + OPERAND(scan), c) == 0) {
				return(0);
			}
			preg->reginput += n;
			break;
		case ANYBUT:
			if (reg_iseol(preg, c) || reg_range_find(preg->program + OPERAND(scan), c) != 0) {
				return(0);
			}
			preg->reginput += n;
			break;
		case NOTHING:
			break;
		case BACK:
			break;
		case BRANCH:
			if (OP(preg, next) != BRANCH)
				next = OPERAND(scan);
			else {
				do {
					save = preg->reginput;
					if (regmatch(preg, OPERAND(scan))) {
						return(1);
					}
					preg->reginput = save;
					scan = regnext(preg, scan);
				} while (scan != 0 && OP(preg, scan) == BRANCH);
				return(0);

			}
			break;
		case REP:
		case REPMIN:
			return regmatchsimplerepeat(preg, scan, OP(preg, scan) == REPMIN);

		case REPX:
		case REPXMIN:
			return regmatchrepeat(preg, scan, OP(preg, scan) == REPXMIN);

		case END:
			return 1;

		case OPENNC:
		case CLOSENC:
			return regmatch(preg, next);

		default:
			if (OP(preg, scan) >= OPEN+1 && OP(preg, scan) < CLOSE_END) {
				save = preg->reginput;
				if (regmatch(preg, next)) {
					if (OP(preg, scan) < CLOSE) {
						int no = OP(preg, scan) - OPEN;
						if (no < preg->nmatch && preg->pmatch[no].rm_so == -1) {
							preg->pmatch[no].rm_so = save - preg->start;
						}
					}
					else {
						int no = OP(preg, scan) - CLOSE;
						if (no < preg->nmatch && preg->pmatch[no].rm_eo == -1) {
							preg->pmatch[no].rm_eo = save - preg->start;
						}
					}
					return(1);
				}
				return(0);
			}
			return REG_ERR_INTERNAL;
		}

		scan = next;
	}

	return REG_ERR_INTERNAL;
}

static int regrepeat(regex_t *preg, int p, int max)
{
	int count = 0;
	const char *scan;
	int opnd;
	int ch;
	int n;

	scan = preg->reginput;
	opnd = OPERAND(p);
	switch (OP(preg, p)) {
	case ANY:

		while (!reg_iseol(preg, *scan) && count < max) {
			count++;
			scan++;
		}
		break;
	case EXACTLY:
		while (count < max) {
			n = reg_utf8_tounicode_case(scan, &ch, preg->cflags & REG_ICASE);
			if (preg->program[opnd] != ch) {
				break;
			}
			count++;
			scan += n;
		}
		break;
	case ANYOF:
		while (count < max) {
			n = reg_utf8_tounicode_case(scan, &ch, preg->cflags & REG_ICASE);
			if (reg_iseol(preg, ch) || reg_range_find(preg->program + opnd, ch) == 0) {
				break;
			}
			count++;
			scan += n;
		}
		break;
	case ANYBUT:
		while (count < max) {
			n = reg_utf8_tounicode_case(scan, &ch, preg->cflags & REG_ICASE);
			if (reg_iseol(preg, ch) || reg_range_find(preg->program + opnd, ch) != 0) {
				break;
			}
			count++;
			scan += n;
		}
		break;
	default:
		preg->err = REG_ERR_INTERNAL;
		count = 0;
		break;
	}
	preg->reginput = scan;

	return(count);
}

static int regnext(regex_t *preg, int p )
{
	int offset;

	offset = NEXT(preg, p);

	if (offset == 0)
		return 0;

	if (OP(preg, p) == BACK)
		return(p-offset);
	else
		return(p+offset);
}

static int regopsize(regex_t *preg, int p )
{

	switch (OP(preg, p)) {
		case REP:
		case REPMIN:
		case REPX:
		case REPXMIN:
			return 5;

		case ANYOF:
		case ANYBUT:
		case EXACTLY: {
			int s = p + 2;
			while (preg->program[s++]) {
			}
			return s - p;
		}
	}
	return 2;
}


size_t regerror(int errcode, const regex_t *preg, char *errbuf,  size_t errbuf_size)
{
	static const char *error_strings[] = {
		"success",
		"no match",
		"bad pattern",
		"null argument",
		"unknown error",
		"too big",
		"out of memory",
		"too many ()",
		"parentheses () not balanced",
		"braces {} not balanced",
		"invalid repetition count(s)",
		"extra characters",
		"*+ of empty atom",
		"nested count",
		"internal error",
		"count follows nothing",
		"trailing backslash",
		"corrupted program",
		"contains null char",
	};
	const char *err;

	if (errcode < 0 || errcode >= REG_ERR_NUM) {
		err = "Bad error code";
	}
	else {
		err = error_strings[errcode];
	}

	return snprintf(errbuf, errbuf_size, "%s", err);
}

void regfree(regex_t *preg)
{
	free(preg->program);
}

#endif
#include <string.h>

void Jim_SetResultErrno(Jim_Interp *interp, const char *msg)
{
    Jim_SetResultFormatted(interp, "%s: %s", msg, strerror(Jim_Errno()));
}

#if defined(__MINGW32__)
#include <sys/stat.h>

int Jim_Errno(void)
{
    switch (GetLastError()) {
    case ERROR_FILE_NOT_FOUND: return ENOENT;
    case ERROR_PATH_NOT_FOUND: return ENOENT;
    case ERROR_TOO_MANY_OPEN_FILES: return EMFILE;
    case ERROR_ACCESS_DENIED: return EACCES;
    case ERROR_INVALID_HANDLE: return EBADF;
    case ERROR_BAD_ENVIRONMENT: return E2BIG;
    case ERROR_BAD_FORMAT: return ENOEXEC;
    case ERROR_INVALID_ACCESS: return EACCES;
    case ERROR_INVALID_DRIVE: return ENOENT;
    case ERROR_CURRENT_DIRECTORY: return EACCES;
    case ERROR_NOT_SAME_DEVICE: return EXDEV;
    case ERROR_NO_MORE_FILES: return ENOENT;
    case ERROR_WRITE_PROTECT: return EROFS;
    case ERROR_BAD_UNIT: return ENXIO;
    case ERROR_NOT_READY: return EBUSY;
    case ERROR_BAD_COMMAND: return EIO;
    case ERROR_CRC: return EIO;
    case ERROR_BAD_LENGTH: return EIO;
    case ERROR_SEEK: return EIO;
    case ERROR_WRITE_FAULT: return EIO;
    case ERROR_READ_FAULT: return EIO;
    case ERROR_GEN_FAILURE: return EIO;
    case ERROR_SHARING_VIOLATION: return EACCES;
    case ERROR_LOCK_VIOLATION: return EACCES;
    case ERROR_SHARING_BUFFER_EXCEEDED: return ENFILE;
    case ERROR_HANDLE_DISK_FULL: return ENOSPC;
    case ERROR_NOT_SUPPORTED: return ENODEV;
    case ERROR_REM_NOT_LIST: return EBUSY;
    case ERROR_DUP_NAME: return EEXIST;
    case ERROR_BAD_NETPATH: return ENOENT;
    case ERROR_NETWORK_BUSY: return EBUSY;
    case ERROR_DEV_NOT_EXIST: return ENODEV;
    case ERROR_TOO_MANY_CMDS: return EAGAIN;
    case ERROR_ADAP_HDW_ERR: return EIO;
    case ERROR_BAD_NET_RESP: return EIO;
    case ERROR_UNEXP_NET_ERR: return EIO;
    case ERROR_NETNAME_DELETED: return ENOENT;
    case ERROR_NETWORK_ACCESS_DENIED: return EACCES;
    case ERROR_BAD_DEV_TYPE: return ENODEV;
    case ERROR_BAD_NET_NAME: return ENOENT;
    case ERROR_TOO_MANY_NAMES: return ENFILE;
    case ERROR_TOO_MANY_SESS: return EIO;
    case ERROR_SHARING_PAUSED: return EAGAIN;
    case ERROR_REDIR_PAUSED: return EAGAIN;
    case ERROR_FILE_EXISTS: return EEXIST;
    case ERROR_CANNOT_MAKE: return ENOSPC;
    case ERROR_OUT_OF_STRUCTURES: return ENFILE;
    case ERROR_ALREADY_ASSIGNED: return EEXIST;
    case ERROR_INVALID_PASSWORD: return EPERM;
    case ERROR_NET_WRITE_FAULT: return EIO;
    case ERROR_NO_PROC_SLOTS: return EAGAIN;
    case ERROR_DISK_CHANGE: return EXDEV;
    case ERROR_BROKEN_PIPE: return EPIPE;
    case ERROR_OPEN_FAILED: return ENOENT;
    case ERROR_DISK_FULL: return ENOSPC;
    case ERROR_NO_MORE_SEARCH_HANDLES: return EMFILE;
    case ERROR_INVALID_TARGET_HANDLE: return EBADF;
    case ERROR_INVALID_NAME: return ENOENT;
    case ERROR_PROC_NOT_FOUND: return ESRCH;
    case ERROR_WAIT_NO_CHILDREN: return ECHILD;
    case ERROR_CHILD_NOT_COMPLETE: return ECHILD;
    case ERROR_DIRECT_ACCESS_HANDLE: return EBADF;
    case ERROR_SEEK_ON_DEVICE: return ESPIPE;
    case ERROR_BUSY_DRIVE: return EAGAIN;
    case ERROR_DIR_NOT_EMPTY: return EEXIST;
    case ERROR_NOT_LOCKED: return EACCES;
    case ERROR_BAD_PATHNAME: return ENOENT;
    case ERROR_LOCK_FAILED: return EACCES;
    case ERROR_ALREADY_EXISTS: return EEXIST;
    case ERROR_FILENAME_EXCED_RANGE: return ENAMETOOLONG;
    case ERROR_BAD_PIPE: return EPIPE;
    case ERROR_PIPE_BUSY: return EAGAIN;
    case ERROR_PIPE_NOT_CONNECTED: return EPIPE;
    case ERROR_DIRECTORY: return ENOTDIR;
    }
    return EINVAL;
}

pidtype waitpid(pidtype pid, int *status, int nohang)
{
    DWORD ret = WaitForSingleObject(pid, nohang ? 0 : INFINITE);
    if (ret == WAIT_TIMEOUT || ret == WAIT_FAILED) {

        return JIM_BAD_PID;
    }
    GetExitCodeProcess(pid, &ret);
    *status = ret;
    CloseHandle(pid);
    return pid;
}

int Jim_MakeTempFile(Jim_Interp *interp, const char *filename_template, int unlink_file)
{
    char name[MAX_PATH];
    HANDLE handle;

    if (!GetTempPath(MAX_PATH, name) || !GetTempFileName(name, filename_template ? filename_template : "JIM", 0, name)) {
        return -1;
    }

    handle = CreateFile(name, GENERIC_READ | GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | (unlink_file ? FILE_FLAG_DELETE_ON_CLOSE : 0),
            NULL);

    if (handle == INVALID_HANDLE_VALUE) {
        goto error;
    }

    Jim_SetResultString(interp, name, -1);
    return _open_osfhandle((int)handle, _O_RDWR | _O_TEXT);

  error:
    Jim_SetResultErrno(interp, name);
    DeleteFile(name);
    return -1;
}

int Jim_OpenForWrite(const char *filename, int append)
{
    if (strcmp(filename, "/dev/null") == 0) {
        filename = "nul:";
    }
    int fd = _open(filename, _O_WRONLY | _O_CREAT | _O_TEXT | (append ? _O_APPEND : _O_TRUNC), _S_IREAD | _S_IWRITE);
    if (fd >= 0 && append) {

        _lseek(fd, 0L, SEEK_END);
    }
    return fd;
}

int Jim_OpenForRead(const char *filename)
{
    if (strcmp(filename, "/dev/null") == 0) {
        filename = "nul:";
    }
    return _open(filename, _O_RDONLY | _O_TEXT, 0);
}

#elif defined(HAVE_UNISTD_H)



int Jim_MakeTempFile(Jim_Interp *interp, const char *filename_template, int unlink_file)
{
    int fd;
    mode_t mask;
    Jim_Obj *filenameObj;

    if (filename_template == NULL) {
        const char *tmpdir = getenv("TMPDIR");
        if (tmpdir == NULL || *tmpdir == '\0' || access(tmpdir, W_OK) != 0) {
            tmpdir = "/tmp/";
        }
        filenameObj = Jim_NewStringObj(interp, tmpdir, -1);
        if (tmpdir[0] && tmpdir[strlen(tmpdir) - 1] != '/') {
            Jim_AppendString(interp, filenameObj, "/", 1);
        }
        Jim_AppendString(interp, filenameObj, "tcl.tmp.XXXXXX", -1);
    }
    else {
        filenameObj = Jim_NewStringObj(interp, filename_template, -1);
    }


    mask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
#ifdef HAVE_MKSTEMP
    fd = mkstemp(filenameObj->bytes);
#else
    if (mktemp(filenameObj->bytes) == NULL) {
        fd = -1;
    }
    else {
        fd = open(filenameObj->bytes, O_RDWR | O_CREAT | O_TRUNC);
    }
#endif
    umask(mask);
    if (fd < 0) {
        Jim_SetResultErrno(interp, Jim_String(filenameObj));
        Jim_FreeNewObj(interp, filenameObj);
        return -1;
    }
    if (unlink_file) {
        remove(Jim_String(filenameObj));
    }

    Jim_SetResult(interp, filenameObj);
    return fd;
}

int Jim_OpenForWrite(const char *filename, int append)
{
    return open(filename, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0666);
}

int Jim_OpenForRead(const char *filename)
{
    return open(filename, O_RDONLY, 0);
}

#endif

#if defined(_WIN32) || defined(WIN32)
#ifndef STRICT
#define STRICT
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#if defined(HAVE_DLOPEN_COMPAT)
void *dlopen(const char *path, int mode)
{
    JIM_NOTUSED(mode);

    return (void *)LoadLibraryA(path);
}

int dlclose(void *handle)
{
    FreeLibrary((HANDLE)handle);
    return 0;
}

void *dlsym(void *handle, const char *symbol)
{
    return GetProcAddress((HMODULE)handle, symbol);
}

char *dlerror(void)
{
    static char msg[121];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
                   LANG_NEUTRAL, msg, sizeof(msg) - 1, NULL);
    return msg;
}
#endif

#ifdef _MSC_VER

#include <sys/timeb.h>


int gettimeofday(struct timeval *tv, void *unused)
{
    struct _timeb tb;

    _ftime(&tb);
    tv->tv_sec = tb.time;
    tv->tv_usec = tb.millitm * 1000;

    return 0;
}


DIR *opendir(const char *name)
{
    DIR *dir = 0;

    if (name && name[0]) {
        size_t base_length = strlen(name);
        const char *all =
            strchr("/\\", name[base_length - 1]) ? "*" : "/*";

        if ((dir = (DIR *) Jim_Alloc(sizeof *dir)) != 0 &&
            (dir->name = (char *)Jim_Alloc(base_length + strlen(all) + 1)) != 0) {
            strcat(strcpy(dir->name, name), all);

            if ((dir->handle = (long)_findfirst(dir->name, &dir->info)) != -1)
                dir->result.d_name = 0;
            else {
                Jim_Free(dir->name);
                Jim_Free(dir);
                dir = 0;
            }
        }
        else {
            Jim_Free(dir);
            dir = 0;
            errno = ENOMEM;
        }
    }
    else {
        errno = EINVAL;
    }
    return dir;
}

int closedir(DIR * dir)
{
    int result = -1;

    if (dir) {
        if (dir->handle != -1)
            result = _findclose(dir->handle);
        Jim_Free(dir->name);
        Jim_Free(dir);
    }
    if (result == -1)
        errno = EBADF;
    return result;
}

struct dirent *readdir(DIR * dir)
{
    struct dirent *result = 0;

    if (dir && dir->handle != -1) {
        if (!dir->result.d_name || _findnext(dir->handle, &dir->info) != -1) {
            result = &dir->result;
            result->d_name = dir->info.name;
        }
    }
    else {
        errno = EBADF;
    }
    return result;
}
#endif
#endif
#include <stdio.h>
#include <signal.h>






#ifndef SIGPIPE
#define SIGPIPE 13
#endif
#ifndef SIGINT
#define SIGINT 2
#endif

const char *Jim_SignalId(int sig)
{
	static char buf[10];
	switch (sig) {
		case SIGINT: return "SIGINT";
		case SIGPIPE: return "SIGPIPE";

	}
	snprintf(buf, sizeof(buf), "%d", sig);
	return buf;
}
#ifndef JIM_BOOTSTRAP_LIB_ONLY
#include <errno.h>
#include <string.h>


#ifdef USE_LINENOISE
#ifdef HAVE_UNISTD_H
    #include <unistd.h>
#endif
#ifdef HAVE_SYS_STAT_H
    #include <sys/stat.h>
#endif
#include "linenoise.h"
#else
#define MAX_LINE_LEN 512
#endif

#ifdef USE_LINENOISE
static void JimCompletionCallback(const char *prefix, linenoiseCompletions *comp, void *userdata);
static const char completion_callback_assoc_key[] = "interactive-completion";
#endif

char *Jim_HistoryGetline(Jim_Interp *interp, const char *prompt)
{
#ifdef USE_LINENOISE
    struct JimCompletionInfo *compinfo = Jim_GetAssocData(interp, completion_callback_assoc_key);
    char *result;
    Jim_Obj *objPtr;
    long mlmode = 0;
    if (compinfo) {
        linenoiseSetCompletionCallback(JimCompletionCallback, compinfo);
    }
    objPtr = Jim_GetVariableStr(interp, "history::multiline", JIM_NONE);
    if (objPtr && Jim_GetLong(interp, objPtr, &mlmode) == JIM_NONE) {
        linenoiseSetMultiLine(mlmode);
    }

    result = linenoise(prompt);

    linenoiseSetCompletionCallback(NULL, NULL);
    return result;
#else
    int len;
    char *line = malloc(MAX_LINE_LEN);

    fputs(prompt, stdout);
    fflush(stdout);

    if (fgets(line, MAX_LINE_LEN, stdin) == NULL) {
        free(line);
        return NULL;
    }
    len = strlen(line);
    if (len && line[len - 1] == '\n') {
        line[len - 1] = '\0';
    }
    return line;
#endif
}

void Jim_HistoryLoad(const char *filename)
{
#ifdef USE_LINENOISE
    linenoiseHistoryLoad(filename);
#endif
}

void Jim_HistoryAdd(const char *line)
{
#ifdef USE_LINENOISE
    linenoiseHistoryAdd(line);
#endif
}

void Jim_HistorySave(const char *filename)
{
#ifdef USE_LINENOISE
#ifdef HAVE_UMASK
    mode_t mask;

    mask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
#endif
    linenoiseHistorySave(filename);
#ifdef HAVE_UMASK
    umask(mask);
#endif
#endif
}

void Jim_HistoryShow(void)
{
#ifdef USE_LINENOISE

    int i;
    int len;
    char **history = linenoiseHistory(&len);
    for (i = 0; i < len; i++) {
        printf("%4d %s\n", i + 1, history[i]);
    }
#endif
}

#ifdef USE_LINENOISE
struct JimCompletionInfo {
    Jim_Interp *interp;
    Jim_Obj *command;
};

static void JimCompletionCallback(const char *prefix, linenoiseCompletions *comp, void *userdata)
{
    struct JimCompletionInfo *info = (struct JimCompletionInfo *)userdata;
    Jim_Obj *objv[2];
    int ret;

    objv[0] = info->command;
    objv[1] = Jim_NewStringObj(info->interp, prefix, -1);

    ret = Jim_EvalObjVector(info->interp, 2, objv);


    if (ret == JIM_OK) {
        int i;
        Jim_Obj *listObj = Jim_GetResult(info->interp);
        int len = Jim_ListLength(info->interp, listObj);
        for (i = 0; i < len; i++) {
            linenoiseAddCompletion(comp, Jim_String(Jim_ListGetIndex(info->interp, listObj, i)));
        }
    }
}

static void JimHistoryFreeCompletion(Jim_Interp *interp, void *data)
{
    struct JimCompletionInfo *compinfo = data;

    Jim_DecrRefCount(interp, compinfo->command);

    Jim_Free(compinfo);
}
#endif

void Jim_HistorySetCompletion(Jim_Interp *interp, Jim_Obj *commandObj)
{
#ifdef USE_LINENOISE
    if (commandObj) {

        Jim_IncrRefCount(commandObj);
    }

    Jim_DeleteAssocData(interp, completion_callback_assoc_key);

    if (commandObj) {
        struct JimCompletionInfo *compinfo = Jim_Alloc(sizeof(*compinfo));
        compinfo->interp = interp;
        compinfo->command = commandObj;

        Jim_SetAssocData(interp, completion_callback_assoc_key, JimHistoryFreeCompletion, compinfo);
    }
#endif
}

int Jim_InteractivePrompt(Jim_Interp *interp)
{
    int retcode = JIM_OK;
    char *history_file = NULL;
#ifdef USE_LINENOISE
    const char *home;

    home = getenv("HOME");
    if (home && isatty(STDIN_FILENO)) {
        int history_len = strlen(home) + sizeof("/.jim_history");
        history_file = Jim_Alloc(history_len);
        snprintf(history_file, history_len, "%s/.jim_history", home);
        Jim_HistoryLoad(history_file);
    }

    Jim_HistorySetCompletion(interp, Jim_NewStringObj(interp, "tcl::autocomplete", -1));
#endif

    printf("Welcome to Jim version %d.%d\n",
        JIM_VERSION / 100, JIM_VERSION % 100);
    Jim_SetVariableStrWithStr(interp, JIM_INTERACTIVE, "1");

    while (1) {
        Jim_Obj *scriptObjPtr;
        const char *result;
        int reslen;
        char prompt[20];

        if (retcode != JIM_OK) {
            const char *retcodestr = Jim_ReturnCode(retcode);

            if (*retcodestr == '?') {
                snprintf(prompt, sizeof(prompt) - 3, "[%d] . ", retcode);
            }
            else {
                snprintf(prompt, sizeof(prompt) - 3, "[%s] . ", retcodestr);
            }
        }
        else {
            strcpy(prompt, ". ");
        }

        scriptObjPtr = Jim_NewStringObj(interp, "", 0);
        Jim_IncrRefCount(scriptObjPtr);
        while (1) {
            char state;
            char *line;

            line = Jim_HistoryGetline(interp, prompt);
            if (line == NULL) {
                if (errno == EINTR) {
                    continue;
                }
                Jim_DecrRefCount(interp, scriptObjPtr);
                retcode = JIM_OK;
                goto out;
            }
            if (Jim_Length(scriptObjPtr) != 0) {

                Jim_AppendString(interp, scriptObjPtr, "\n", 1);
            }
            Jim_AppendString(interp, scriptObjPtr, line, -1);
            free(line);
            if (Jim_ScriptIsComplete(interp, scriptObjPtr, &state))
                break;

            snprintf(prompt, sizeof(prompt), "%c> ", state);
        }
#ifdef USE_LINENOISE
        if (strcmp(Jim_String(scriptObjPtr), "h") == 0) {

            Jim_HistoryShow();
            Jim_DecrRefCount(interp, scriptObjPtr);
            continue;
        }

        Jim_HistoryAdd(Jim_String(scriptObjPtr));
        if (history_file) {
            Jim_HistorySave(history_file);
        }
#endif
        retcode = Jim_EvalObj(interp, scriptObjPtr);
        Jim_DecrRefCount(interp, scriptObjPtr);

        if (retcode == JIM_EXIT) {
            break;
        }
        if (retcode == JIM_ERR) {
            Jim_MakeErrorMessage(interp);
        }
        result = Jim_GetString(Jim_GetResult(interp), &reslen);
        if (reslen) {
            printf("%s\n", result);
        }
    }
  out:
    Jim_Free(history_file);

    return retcode;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



extern int Jim_initjimshInit(Jim_Interp *interp);

static void JimSetArgv(Jim_Interp *interp, int argc, char *const argv[])
{
    int n;
    Jim_Obj *listObj = Jim_NewListObj(interp, NULL, 0);


    for (n = 0; n < argc; n++) {
        Jim_Obj *obj = Jim_NewStringObj(interp, argv[n], -1);

        Jim_ListAppendElement(interp, listObj, obj);
    }

    Jim_SetVariableStr(interp, "argv", listObj);
    Jim_SetVariableStr(interp, "argc", Jim_NewIntObj(interp, argc));
}

static void JimPrintErrorMessage(Jim_Interp *interp)
{
    Jim_MakeErrorMessage(interp);
    fprintf(stderr, "%s\n", Jim_String(Jim_GetResult(interp)));
}

void usage(const char* executable_name)
{
    printf("jimsh version %d.%d\n", JIM_VERSION / 100, JIM_VERSION % 100);
    printf("Usage: %s\n", executable_name);
    printf("or   : %s [options] [filename]\n", executable_name);
    printf("\n");
    printf("Without options: Interactive mode\n");
    printf("\n");
    printf("Options:\n");
    printf("      --version  : prints the version string\n");
    printf("      --help     : prints this text\n");
    printf("      -e CMD     : executes command CMD\n");
    printf("                   NOTE: all subsequent options will be passed as arguments to the command\n");
    printf("    [filename|-] : executes the script contained in the named file, or from stdin if \"-\"\n");
    printf("                   NOTE: all subsequent options will be passed to the script\n\n");
}

int main(int argc, char *const argv[])
{
    int retcode;
    Jim_Interp *interp;
    char *const orig_argv0 = argv[0];


    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("%d.%d\n", JIM_VERSION / 100, JIM_VERSION % 100);
        return 0;
    }
    else if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        usage(argv[0]);
        return 0;
    }


    interp = Jim_CreateInterp();
    Jim_RegisterCoreCommands(interp);


    if (Jim_InitStaticExtensions(interp) != JIM_OK) {
        JimPrintErrorMessage(interp);
    }

    Jim_SetVariableStrWithStr(interp, "jim::argv0", orig_argv0);
    Jim_SetVariableStrWithStr(interp, JIM_INTERACTIVE, argc == 1 ? "1" : "0");
    retcode = Jim_initjimshInit(interp);

    if (argc == 1) {

        if (retcode == JIM_ERR) {
            JimPrintErrorMessage(interp);
        }
        if (retcode != JIM_EXIT) {
            JimSetArgv(interp, 0, NULL);
            retcode = Jim_InteractivePrompt(interp);
        }
    }
    else {

        if (argc > 2 && strcmp(argv[1], "-e") == 0) {

            JimSetArgv(interp, argc - 3, argv + 3);
            retcode = Jim_Eval(interp, argv[2]);
            if (retcode != JIM_ERR) {
                printf("%s\n", Jim_String(Jim_GetResult(interp)));
            }
        }
        else {
            Jim_SetVariableStr(interp, "argv0", Jim_NewStringObj(interp, argv[1], -1));
            JimSetArgv(interp, argc - 2, argv + 2);
            if (strcmp(argv[1], "-") == 0) {
                retcode = Jim_Eval(interp, "eval [info source [stdin read] stdin 1]");
            } else {
                retcode = Jim_EvalFile(interp, argv[1]);
            }
        }
        if (retcode == JIM_ERR) {
            JimPrintErrorMessage(interp);
        }
    }
    if (retcode == JIM_EXIT) {
        retcode = Jim_GetExitCode(interp);
    }
    else if (retcode == JIM_ERR) {
        retcode = 1;
    }
    else {
        retcode = 0;
    }
    Jim_FreeInterp(interp);
    return retcode;
}
#endif
