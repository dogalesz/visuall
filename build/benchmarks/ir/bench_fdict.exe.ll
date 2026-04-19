; ModuleID = 'bench_fdict.vsl'
source_filename = "bench_fdict.vsl"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-w64-windows-gnu"

@str = private unnamed_addr constant [6 x i8] c"alpha\00", align 1
@str.1 = private unnamed_addr constant [5 x i8] c"beta\00", align 1
@str.2 = private unnamed_addr constant [6 x i8] c"gamma\00", align 1
@str.3 = private unnamed_addr constant [6 x i8] c"delta\00", align 1
@str.4 = private unnamed_addr constant [8 x i8] c"epsilon\00", align 1

declare void @__visuall_print_int(i64) local_unnamed_addr

declare void @__visuall_print_newline() local_unnamed_addr

declare ptr @__visuall_dict_new() local_unnamed_addr

declare void @__visuall_dict_set(ptr, ptr, i64) local_unnamed_addr

declare void @__visuall_gc_init(ptr) local_unnamed_addr

declare void @__visuall_gc_shutdown() local_unnamed_addr

define noundef i32 @main() local_unnamed_addr {
entry:
  %gc.anchor = alloca i8, align 1
  call void @__visuall_gc_init(ptr nonnull %gc.anchor)
  br label %while.body.i

while.body.i:                                     ; preds = %while.body.i, %entry
  %i.027.i = phi i64 [ 0, %entry ], [ %add.i, %while.body.i ]
  %dict.new.i = call ptr @__visuall_dict_new()
  call void @__visuall_dict_set(ptr %dict.new.i, ptr nonnull @str, i64 %i.027.i)
  %add.i = add nuw nsw i64 %i.027.i, 1
  call void @__visuall_dict_set(ptr %dict.new.i, ptr nonnull @str.1, i64 %add.i)
  %add7.i = add nuw nsw i64 %i.027.i, 2
  call void @__visuall_dict_set(ptr %dict.new.i, ptr nonnull @str.2, i64 %add7.i)
  %add9.i = add nuw nsw i64 %i.027.i, 3
  call void @__visuall_dict_set(ptr %dict.new.i, ptr nonnull @str.3, i64 %add9.i)
  %add11.i = add nuw nsw i64 %i.027.i, 4
  call void @__visuall_dict_set(ptr %dict.new.i, ptr nonnull @str.4, i64 %add11.i)
  %exitcond.not.i = icmp eq i64 %add.i, 200000
  br i1 %exitcond.not.i, label %bench.exit, label %while.body.i

bench.exit:                                       ; preds = %while.body.i
  call void @__visuall_print_int(i64 19999900000)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}
