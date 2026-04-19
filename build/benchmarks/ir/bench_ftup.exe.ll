; ModuleID = 'bench_ftup.vsl'
source_filename = "bench_ftup.vsl"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-w64-windows-gnu"

declare void @__visuall_print_int(i64) local_unnamed_addr

declare void @__visuall_print_newline() local_unnamed_addr

declare void @__visuall_list_push(ptr, i64) local_unnamed_addr

declare i64 @__visuall_list_get(ptr, i64) local_unnamed_addr

declare ptr @__visuall_tuple_new(i64) local_unnamed_addr

declare void @__visuall_gc_init(ptr) local_unnamed_addr

declare void @__visuall_gc_shutdown() local_unnamed_addr

define noundef i32 @main() local_unnamed_addr {
entry:
  %gc.anchor = alloca i8, align 1
  call void @__visuall_gc_init(ptr nonnull %gc.anchor)
  br label %while.body.i

while.body.i:                                     ; preds = %while.body.i, %entry
  %i.026.i = phi i64 [ 0, %entry ], [ %add18.i, %while.body.i ]
  %total.025.i = phi i64 [ 0, %entry ], [ %add16.i, %while.body.i ]
  %tuple.new.i = call ptr @__visuall_tuple_new(i64 3)
  call void @__visuall_list_push(ptr %tuple.new.i, i64 %i.026.i)
  %mul.i = shl nuw nsw i64 %i.026.i, 1
  call void @__visuall_list_push(ptr %tuple.new.i, i64 %mul.i)
  %mul7.i = mul nuw nsw i64 %i.026.i, 3
  call void @__visuall_list_push(ptr %tuple.new.i, i64 %mul7.i)
  %unpack.elem.i = call i64 @__visuall_list_get(ptr %tuple.new.i, i64 0)
  %unpack.elem9.i = call i64 @__visuall_list_get(ptr %tuple.new.i, i64 1)
  %unpack.elem10.i = call i64 @__visuall_list_get(ptr %tuple.new.i, i64 2)
  %add.i = add i64 %unpack.elem.i, %total.025.i
  %add14.i = add i64 %add.i, %unpack.elem9.i
  %add16.i = add i64 %add14.i, %unpack.elem10.i
  %add18.i = add nuw nsw i64 %i.026.i, 1
  %exitcond.not.i = icmp eq i64 %add18.i, 500000
  br i1 %exitcond.not.i, label %bench.exit, label %while.body.i

bench.exit:                                       ; preds = %while.body.i
  call void @__visuall_print_int(i64 %add16.i)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}
