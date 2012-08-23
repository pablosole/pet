/*
 * copyright 2012 sam l'ecuyer
 */

#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <Shlwapi.h>
#include <stdio.h>
#include <malloc.h>
#include <tchar.h> 
#include <wchar.h> 
#include <strsafe.h>
#include "sorrow.h"

namespace sorrow {
	using namespace v8;
    
    /**
     *  File functions for A/0
     */
    
	V8_FUNCTN(OpenRaw) {
		return THROW(ERR(V8_STR("Unimplemented")))
	}
	
    // only implements part 1
	V8_FUNCTN(Move) {
		HandleScope scope;
		if (args.Length() != 2) {
            return THROW(ERR(V8_STR("This method must take two arguments")))
        }
		String::Value source(args[0]);
        String::Value target(args[1]);

		int mv = MoveFile((WCHAR *)*source, (WCHAR *)*target);
		if (mv == 0) {
			return THROW(ERR(V8_STR("Could not move file")))
		}
		return Undefined();
	}
	
	V8_FUNCTN(Remove) {
		HandleScope scope;
		if (args.Length() != 1) {
            return THROW(ERR(V8_STR("This method must take one argument")))
        }
        String::Value path(args[0]);
		int rm = DeleteFile((WCHAR *)*path);
		if (rm == 0) {
			return THROW(ERR(V8_STR("Could not remove file")))
		}
		return Undefined();
	}
	
	V8_FUNCTN(Touch) {
        HandleScope scope;
        if (args.Length() < 1) {
            return THROW(ERR(V8_STR("This method must take at least one argument")))
        } 

        if (args.Length() > 1) {
			return THROW(ERR(V8_STR("Date support unimplemented")))
		}
        String::Value path(args[0]);
        
		HANDLE fhandle = CreateFile((WCHAR *)*path, 0, 0, 0, OPEN_ALWAYS | CREATE_NEW, 0, 0);
        if (fhandle == INVALID_HANDLE_VALUE) {
           return THROW(ERR(V8_STR("Could not touch file"))) 
        }
		CloseHandle(fhandle);

        return Undefined();
	}
    
    
    /**
     *  Directory functions for A/0
     */
    
	V8_FUNCTN(MakeDirectory) {
        HandleScope scope;
		if (args.Length() < 1) {
            return THROW(ERR(V8_STR("This method must take at least one argument")))
        }
        String::Value path(args[0]->ToString());
        uint32_t mod = 0777;
        if (args.Length() == 2 && args[1]->IsUint32()) {
            mod = args[1]->Uint32Value();
        }
        int status = CreateDirectory((WCHAR *)*path, 0);
        if (status == 0) {
            return THROW(ERR(V8_STR("Could not create directory")))
        }
        return Undefined();
	}
	
	V8_FUNCTN(RemoveDirectory) {
		HandleScope scope;
		if (args.Length() != 1) {
            return THROW(ERR(V8_STR("This method must take one argument")))
        }
        String::Value path(args[0]->ToString());
		int status = ::RemoveDirectory((WCHAR *)*path);
        if (status == 0) {
            return THROW(ERR(V8_STR("Could not remove directory")))
        }
        return Undefined();
	}
	
	
    /**
     *  Path functions for A/0
     */
	V8_FUNCTN(Canonical) {
		HandleScope scope;
		WCHAR pathout[MAX_PATH];
		if (args.Length() != 1) {
            return THROW(ERR(V8_STR("This method must take one argument")))
        }
        String::Value path(args[0]->ToString());
		BOOL ret = PathCanonicalize(pathout, (WCHAR *)*path);
		if (ret == FALSE) {
			return Undefined();
		}

		Local<String> retstr = V8_STR((uint16_t *)pathout);
		return scope.Close(retstr);
	}
    
    // workingDirectory
	V8_FUNCTN(WorkingDirectory) {
		HandleScope scope;
        size_t size = GetCurrentDirectory(0, NULL);
		if (!size) {
			return Undefined();
		}

        WCHAR *path = new WCHAR[size];
        size = GetCurrentDirectory(size, path);
		if (!size) {
			return Undefined();
		}

		Local<String> retstr = V8_STR((uint16_t *)path);
		delete[] path;
		return scope.Close(retstr);
    }
    
    // changeWorkingDirectory
    V8_FUNCTN(ChangeWorkingDirectory) {
        HandleScope scope;
        String::Value path(args[0]);
        BOOL ret = SetCurrentDirectory((WCHAR *)*path);
        if (!ret) {
            return THROW(ERR(V8_STR("Could not change working directory")))
        }
		return Undefined();
    }
    
    
    /**
     *  Test functions for A/0
     */
    
	V8_FUNCTN(Exists) {
		HandleScope scope;
		if (args.Length() != 1) {
            return THROW(ERR(V8_STR("This method must take one argument")))
        }
        String::Value path(args[0]->ToString());
		DWORD status = GetFileAttributes((WCHAR *)*path);
        return Boolean::New(status != INVALID_FILE_ATTRIBUTES);
	}
	
	V8_FUNCTN(IsFile) {
		HandleScope scope;
		if (args.Length() != 1) {
            return THROW(ERR(V8_STR("This method must take one argument")))
        }
        String::Value path(args[0]->ToString());
		DWORD status = GetFileAttributes((WCHAR *)*path);
        if (status == INVALID_FILE_ATTRIBUTES) return False();
        return Boolean::New((status & FILE_ATTRIBUTE_DIRECTORY) == 0);
	}
    
    V8_FUNCTN(IsDirectory) {
		HandleScope scope;
		if (args.Length() != 1) {
            return THROW(ERR(V8_STR("This method must take one argument")))
        }
        String::Value path(args[0]->ToString());
		DWORD status = GetFileAttributes((WCHAR *)*path);
        if (status == INVALID_FILE_ATTRIBUTES) return False();
        return Boolean::New((status & FILE_ATTRIBUTE_DIRECTORY) != 0);
	}
    
    V8_FUNCTN(IsReadable) {
		HandleScope scope;
		if (args.Length() != 1) {
            return THROW(ERR(V8_STR("This method must take one argument")))
        }
        String::Value path(args[0]->ToString());
		HANDLE f = CreateFile((WCHAR *)*path, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
		if (f != INVALID_HANDLE_VALUE)
			CloseHandle(f);
        
        return Boolean::New(f != INVALID_HANDLE_VALUE);
	}
    
    V8_FUNCTN(IsWriteable) {
        HandleScope scope;
		if (args.Length() != 1) {
            return THROW(ERR(V8_STR("This method must take one argument")))
        }
        String::Value path(args[0]->ToString());
		HANDLE f = CreateFile((WCHAR *)*path, GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
		if (f != INVALID_HANDLE_VALUE)
			CloseHandle(f);
        
        return Boolean::New(f != INVALID_HANDLE_VALUE);
	}
    
    V8_FUNCTN(Same) {
		return THROW(ERR(V8_STR("Unimplemented")))
	}
    
    V8_FUNCTN(SameFilesystem) {
		return THROW(ERR(V8_STR("Unimplemented")))
	}
    
    
    /**
     *  Attribute functions for A/0
     */
    
	V8_FUNCTN(Size) {
		HandleScope scope;
		if (args.Length() != 1) {
            return THROW(ERR(V8_STR("This method must take one argument")))
        }
        String::Value path(args[0]->ToString());
		HANDLE f = CreateFile((WCHAR *)*path, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
		if (f == INVALID_HANDLE_VALUE) {
            return THROW(ERR(V8_STR("Could not get size")))
		}

		LARGE_INTEGER size;
		if (GetFileSizeEx(f, &size) == 0) {
            return THROW(ERR(V8_STR("Could not get size")))
        }
        return Number::New(double(size.QuadPart));
	}
	
	V8_FUNCTN(LastModified) {
		return THROW(ERR(V8_STR("Unimplemented")))
	}
    
    
    /**
     *  Listing functions for A/0
     */
    
	V8_FUNCTN(List) {
		HandleScope scope;
		if (args.Length() != 1) {
            return THROW(ERR(V8_STR("This method must take one argument")))
        }
        String::Value path(args[0]->ToString());
		WIN32_FIND_DATA FindFileData;
		HANDLE hFind = INVALID_HANDLE_VALUE;
		DWORD dwError;
		LPWSTR DirSpec;
		size_t length_of_path;

		DirSpec = (LPWSTR) malloc(MAX_PATH * 2);
		if (!DirSpec) {
			return THROW(ERR(V8_STR("Insufficient memory")))
		}

		StringCbLength((WCHAR *)*path, MAX_PATH, &length_of_path);
		if (length_of_path > (MAX_PATH - 2)) {
			free(DirSpec);
			return THROW(ERR(V8_STR("Pathname too long")))
		}

		StringCbCopyN(DirSpec, MAX_PATH, (WCHAR *)*path, length_of_path+1);
		StringCbCatN(DirSpec, MAX_PATH, L"\\*", 2*sizeof(WCHAR));
		hFind = FindFirstFile(DirSpec, &FindFileData);

        if (hFind == INVALID_HANDLE_VALUE) {
			free(DirSpec);
            return THROW(ERR(V8_STR("Could not read directory")))
		}

        Local<Array> list = Array::New();
        uint32_t count = 0;
        list->Set(Integer::New(count++), V8_STR((uint16_t *)&FindFileData.cFileName));
		
		while (FindNextFile(hFind, &FindFileData) != 0) {
			list->Set(Integer::New(count++), V8_STR((uint16_t *)&FindFileData.cFileName));
        }

		FindClose(hFind);
		free(DirSpec);
		dwError = GetLastError();
		if (dwError != ERROR_NO_MORE_FILES) {
            return THROW(ERR(V8_STR("Could not read directory")))
		}

        return scope.Close(list);
	}
	
	V8_FUNCTN(Iterate) {
		return THROW(ERR(V8_STR("Unimplemented")))
	}
    
    V8_FUNCTN(Next) {
		return THROW(ERR(V8_STR("Unimplemented")))
	}
    
    V8_FUNCTN(Iterator) {
		return THROW(ERR(V8_STR("Unimplemented")))
	}
    
    V8_FUNCTN(Close) {
		return THROW(ERR(V8_STR("Unimplemented")))
	}
    
    V8_FUNCTN(Owner) {
		return THROW(ERR(V8_STR("Unimplemented")))
	}
    
    V8_FUNCTN(ChangeOwner) {
		return THROW(ERR(V8_STR("Unimplemented")))
	}
    
    V8_FUNCTN(Group) {
		return THROW(ERR(V8_STR("Unimplemented")))
	}
    
    V8_FUNCTN(ChangeGroup) {
		return THROW(ERR(V8_STR("Unimplemented")))
	}
    
    V8_FUNCTN(Permissions) {
		return THROW(ERR(V8_STR("Unimplemented")))
	}
    
    V8_FUNCTN(ChangePermissions) {
		return THROW(ERR(V8_STR("Unimplemented")))
	}
    
    V8_FUNCTN(SymbolicLink) {
		return THROW(ERR(V8_STR("Unimplemented")))
	}
    
    V8_FUNCTN(HardLink) {
		return THROW(ERR(V8_STR("Unimplemented")))
	}
    
    V8_FUNCTN(ReadLink) {
		return THROW(ERR(V8_STR("Unimplemented")))
	}
    
    V8_FUNCTN(IsLink) {
		return THROW(ERR(V8_STR("Unimplemented")))
	}
    
    namespace Filesystem {    
    void Initialize(Handle<Object> internals) {
		HandleScope scope;
        Local<Object> fsObj = Object::New();
        
        SET_METHOD(fsObj, "workingDirectory",    WorkingDirectory)
        SET_METHOD(fsObj, "changeWorkingDirectory",    ChangeWorkingDirectory)
		
		SET_METHOD(fsObj, "openRaw",    OpenRaw)
        SET_METHOD(fsObj, "move",       Move)
        SET_METHOD(fsObj, "remove",     Remove)
        SET_METHOD(fsObj, "touch",      Touch)
        SET_METHOD(fsObj, "mkdir",      MakeDirectory)
        SET_METHOD(fsObj, "rmdir",      RemoveDirectory)
        SET_METHOD(fsObj, "canonical",  Canonical)
        SET_METHOD(fsObj, "owner",      Owner)
        SET_METHOD(fsObj, "chown",      ChangeOwner)
        SET_METHOD(fsObj, "group",      Group)
        SET_METHOD(fsObj, "chgroup",    ChangeGroup)
        SET_METHOD(fsObj, "perm",       Permissions)
        SET_METHOD(fsObj, "chperm",     ChangePermissions)
        
        SET_METHOD(fsObj, "slink",      SymbolicLink)
        SET_METHOD(fsObj, "hlink",      HardLink)
        SET_METHOD(fsObj, "rlink",      ReadLink)
        
        SET_METHOD(fsObj, "exists",     Exists)
        SET_METHOD(fsObj, "isfile",     IsFile)
        SET_METHOD(fsObj, "isdir",      IsDirectory)
        SET_METHOD(fsObj, "islink",     IsLink)
        SET_METHOD(fsObj, "isreadable", IsReadable)
        SET_METHOD(fsObj, "iswriteable",IsWriteable)
        SET_METHOD(fsObj, "same",       Same)
        SET_METHOD(fsObj, "samefs",     SameFilesystem)
        
        SET_METHOD(fsObj, "size",       Size)
        SET_METHOD(fsObj, "lastmod",    LastModified)
        
        SET_METHOD(fsObj, "list",       List)
        
        internals->Set(V8_STR("fs"), fsObj);
    }
    }
	
} // namespce sorrow