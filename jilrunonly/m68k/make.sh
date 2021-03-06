# build core files
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/bind_arraylist.c -o ./bind_arraylist.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/bind_runtime.c -o ./bind_runtime.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/bind_runtime_exception.c -o ./bind_runtime_exception.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/bind_stringMatch.c -o ./bind_stringMatch.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jclarray.c -o ./jclarray.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jclclass.c -o ./jclclass.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jclclause.c -o ./jclclause.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jclerrors.c -o ./jclerrors.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jclfile.c -o ./jclfile.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jclfunc.c -o ./jclfunc.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jclgendoc.c -o ./jclgendoc.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jcllinker.c -o ./jcllinker.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jclnative.c -o ./jclnative.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jcloption.c -o ./jcloption.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jclpair.c -o ./jclpair.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jclstate.c -o ./jclstate.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jclstring.c -o ./jclstring.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jclvar.c -o ./jclvar.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jilallocators.c -o ./jilallocators.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jilarray.c -o ./jilarray.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jilarraylist.c -o ./jilarraylist.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jilchunk.c -o ./jilchunk.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jilcodelist.c -o ./jilcodelist.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jilcompiler.c -o ./jilcompiler.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jilcstrsegment.c -o ./jilcstrsegment.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jildebug.c -o ./jildebug.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jilexception.c -o ./jilexception.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jilexecbytecode.c -o ./jilexecbytecode.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jilfixmem.c -o ./jilfixmem.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jilhandle.c -o ./jilhandle.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jiliterator.c -o ./jiliterator.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jillist.c -o ./jillist.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jilmachine.c -o ./jilmachine.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jilnativetype.c -o ./jilnativetype.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jiloptables.c -o ./jiloptables.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jilprogramming.c -o ./jilprogramming.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jilruntime.c -o ./jilruntime.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jilstdinc.c -o ./jilstdinc.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jilstring.c -o ./jilstring.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jilsymboltable.c -o ./jilsymboltable.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jiltable.c -o ./jiltable.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jiltools.c -o ./jiltools.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jiltypeinfo.c -o ./jiltypeinfo.o -I ../../jilruntime/include -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../../jilruntime/src/jiltypelist.c -o ./jiltypelist.o -I ../../jilruntime/include -Ofast
# build command line interpreter
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../contrib/native/ansi/ntl_file.c -o ./ntl_file.o -I ../../jilruntime/include -I ../../jilruntime/src -I ../contrib/native/ansi -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../contrib/native/ansi/ntl_math.c -o ./ntl_math.o -I ../../jilruntime/include -I ../../jilruntime/src -I ../contrib/native/ansi -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../contrib/native/ansi/ntl_stdlib.c -o ./ntl_stdlib.o -I ../../jilruntime/include -I ../../jilruntime/src -I ../contrib/native/ansi -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../contrib/native/ansi/ntl_time.c -o ./ntl_time.o -I ../../jilruntime/include -I ../../jilruntime/src -I ../contrib/native/ansi -Ofast
m68k-atari-mint-gcc -D JIL_MACHINE_NO_64_BIT -D JIL_STRING_POOLING=0 -c ../src/main.c -o ./main.o -I ../../jilruntime/include -I ../../jilruntime/src -I ../contrib/native/ansi -Ofast
# link
m68k-atari-mint-gcc bind_arraylist.o bind_runtime.o bind_runtime_exception.o bind_stringMatch.o jclarray.o jclclass.o jclclause.o jclerrors.o jclfile.o jclfunc.o jclgendoc.o jcllinker.o jclnative.o jcloption.o jclpair.o jclstate.o jclstring.o jclvar.o jilallocators.o jilarray.o jilarraylist.o jilchunk.o jilcodelist.o jilcompiler.o jilcstrsegment.o jildebug.o jilexception.o jilexecbytecode.o jilfixmem.o jilhandle.o jiliterator.o jillist.o jilmachine.o jilnativetype.o jiloptables.o jilprogramming.o jilruntime.o jilstdinc.o jilstring.o jilsymboltable.o jiltable.o jiltools.o jiltypeinfo.o jiltypelist.o ntl_file.o ntl_math.o ntl_stdlib.o ntl_time.o main.o -lc -lm -o jilrun.tos -Ofast
