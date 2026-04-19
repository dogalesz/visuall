; ModuleID = 'bench_fcmp.vsl'
source_filename = "bench_fcmp.vsl"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-w64-windows-gnu"

declare void @__visuall_print_int(i64) local_unnamed_addr

declare void @__visuall_print_newline() local_unnamed_addr

declare ptr @__visuall_list_new() local_unnamed_addr

declare void @__visuall_list_push(ptr, i64) local_unnamed_addr

declare i64 @__visuall_list_get(ptr, i64) local_unnamed_addr

declare i64 @__visuall_list_len(ptr) local_unnamed_addr

declare ptr @__visuall_range(i64, i64, i64) local_unnamed_addr

declare void @__visuall_gc_init(ptr) local_unnamed_addr

declare void @__visuall_gc_shutdown() local_unnamed_addr

define noundef i32 @main() local_unnamed_addr {
entry:
  %gc.anchor = alloca i8, align 1
  call void @__visuall_gc_init(ptr nonnull %gc.anchor)
  br label %while.body.i

while.body.i:                                     ; preds = %comp.end.i, %entry
  %total.024.i = phi i64 [ 0, %entry ], [ %add11.i, %comp.end.i ]
  %i.023.i = phi i64 [ 0, %entry ], [ %add13.i, %comp.end.i ]
  %comp.list.i = call ptr @__visuall_list_new()
  %range.result.i = call ptr @__visuall_range(i64 0, i64 100, i64 1)
  %comp.len.i = call i64 @__visuall_list_len(ptr %range.result.i)
  %comp.cmp21.i = icmp sgt i64 %comp.len.i, 0
  br i1 %comp.cmp21.i, label %comp.body.i, label %comp.end.i

comp.body.i:                                      ; preds = %while.body.i, %comp.skip.i
  %__comp_idx.022.i = phi i64 [ %comp.inc.i, %comp.skip.i ], [ 0, %while.body.i ]
  %comp.elem.i = call i64 @__visuall_list_get(ptr %range.result.i, i64 %__comp_idx.022.i)
  %0 = and i64 %comp.elem.i, 1
  %eq.i = icmp eq i64 %0, 0
  br i1 %eq.i, label %comp.push.i, label %comp.skip.i

comp.end.i:                                       ; preds = %comp.skip.i, %while.body.i
  %idx.get.i = call i64 @__visuall_list_get(ptr %comp.list.i, i64 0)
  %add.i = add i64 %idx.get.i, %total.024.i
  %idx.get10.i = call i64 @__visuall_list_get(ptr %comp.list.i, i64 49)
  %add11.i = add i64 %add.i, %idx.get10.i
  %add13.i = add nuw nsw i64 %i.023.i, 1
  %exitcond25.not.i = icmp eq i64 %add13.i, 10000
  br i1 %exitcond25.not.i, label %bench.exit, label %while.body.i

comp.push.i:                                      ; preds = %comp.body.i
  %mul.i = mul i64 %comp.elem.i, %comp.elem.i
  call void @__visuall_list_push(ptr %comp.list.i, i64 %mul.i)
  br label %comp.skip.i

comp.skip.i:                                      ; preds = %comp.push.i, %comp.body.i
  %comp.inc.i = add nuw nsw i64 %__comp_idx.022.i, 1
  %exitcond.not.i = icmp eq i64 %comp.inc.i, %comp.len.i
  br i1 %exitcond.not.i, label %comp.end.i, label %comp.body.i

bench.exit:                                       ; preds = %comp.end.i
  call void @__visuall_print_int(i64 %add11.i)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}
