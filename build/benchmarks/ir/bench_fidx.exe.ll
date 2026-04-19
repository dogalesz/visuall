; ModuleID = 'bench_fidx.vsl'
source_filename = "bench_fidx.vsl"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-w64-windows-gnu"

declare void @__visuall_print_int(i64) local_unnamed_addr

declare void @__visuall_print_newline() local_unnamed_addr

declare i64 @__visuall_list_get(ptr, i64) local_unnamed_addr

declare void @__visuall_list_set(ptr, i64, i64) local_unnamed_addr

declare ptr @__visuall_range(i64, i64, i64) local_unnamed_addr

declare void @__visuall_gc_init(ptr) local_unnamed_addr

declare void @__visuall_gc_shutdown() local_unnamed_addr

define noundef i32 @main() local_unnamed_addr {
entry:
  %gc.anchor = alloca i8, align 1
  call void @__visuall_gc_init(ptr nonnull %gc.anchor)
  %range.result.i = call ptr @__visuall_range(i64 0, i64 10000, i64 1)
  br label %while.body.i

while.body.i:                                     ; preds = %while.body.i, %entry
  %i.034.i = phi i64 [ 0, %entry ], [ %add10.i, %while.body.i ]
  %mul.i = mul nuw nsw i64 %i.034.i, 3
  %add.i = add nuw nsw i64 %mul.i, 1
  %mod.lhs.trunc.i = trunc nuw i64 %i.034.i to i32
  %mod33.i = urem i32 %mod.lhs.trunc.i, 10000
  %mod.zext.i = zext nneg i32 %mod33.i to i64
  call void @__visuall_list_set(ptr %range.result.i, i64 %mod.zext.i, i64 %add.i)
  %add10.i = add nuw nsw i64 %i.034.i, 1
  %exitcond.not.i = icmp eq i64 %add10.i, 2000000
  br i1 %exitcond.not.i, label %while.body12.i, label %while.body.i

while.body12.i:                                   ; preds = %while.body.i, %while.body12.i
  %total.036.i = phi i64 [ %add20.i, %while.body12.i ], [ 0, %while.body.i ]
  %i.135.i = phi i64 [ %add22.i, %while.body12.i ], [ 0, %while.body.i ]
  %idx.get.i = call i64 @__visuall_list_get(ptr %range.result.i, i64 %i.135.i)
  %add20.i = add i64 %idx.get.i, %total.036.i
  %add22.i = add nuw nsw i64 %i.135.i, 1
  %exitcond37.not.i = icmp eq i64 %add22.i, 10000
  br i1 %exitcond37.not.i, label %bench.exit, label %while.body12.i

bench.exit:                                       ; preds = %while.body12.i
  call void @__visuall_print_int(i64 %add20.i)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}
