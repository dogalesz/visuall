; ModuleID = 'bench_fslc.vsl'
source_filename = "bench_fslc.vsl"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-w64-windows-gnu"

declare void @__visuall_print_int(i64) local_unnamed_addr

declare void @__visuall_print_newline() local_unnamed_addr

declare i64 @__visuall_list_get(ptr, i64) local_unnamed_addr

declare ptr @__visuall_list_slice(ptr, i64, i64, i64) local_unnamed_addr

declare ptr @__visuall_range(i64, i64, i64) local_unnamed_addr

declare void @__visuall_gc_init(ptr) local_unnamed_addr

declare void @__visuall_gc_shutdown() local_unnamed_addr

define noundef i32 @main() local_unnamed_addr {
entry:
  %gc.anchor = alloca i8, align 1
  call void @__visuall_gc_init(ptr nonnull %gc.anchor)
  %range.result.i = call ptr @__visuall_range(i64 0, i64 1000, i64 1)
  br label %while.body.i

while.body.i:                                     ; preds = %while.body.i, %entry
  %i.020.i = phi i64 [ 0, %entry ], [ %add12.i, %while.body.i ]
  %total.019.i = phi i64 [ 0, %entry ], [ %add10.i, %while.body.i ]
  %mod.lhs.trunc.i = trunc nuw i64 %i.020.i to i16
  %mod18.i = urem i16 %mod.lhs.trunc.i, 800
  %mod.zext.i = zext nneg i16 %mod18.i to i64
  %add.i = add nuw nsw i64 %mod.zext.i, 200
  %slice.result.i = call ptr @__visuall_list_slice(ptr %range.result.i, i64 %mod.zext.i, i64 %add.i, i64 1)
  %idx.get.i = call i64 @__visuall_list_get(ptr %slice.result.i, i64 0)
  %add10.i = add i64 %idx.get.i, %total.019.i
  %add12.i = add nuw nsw i64 %i.020.i, 1
  %exitcond.not.i = icmp eq i64 %add12.i, 50000
  br i1 %exitcond.not.i, label %bench.exit, label %while.body.i

bench.exit:                                       ; preds = %while.body.i
  call void @__visuall_print_int(i64 %add10.i)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}
