; ModuleID = 'bench_nest.vsl'
source_filename = "bench_nest.vsl"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-w64-windows-gnu"

declare void @__visuall_print_int(i64) local_unnamed_addr

declare void @__visuall_print_newline() local_unnamed_addr

declare void @__visuall_gc_init(ptr) local_unnamed_addr

declare void @__visuall_gc_shutdown() local_unnamed_addr

define noundef i32 @main() local_unnamed_addr {
entry:
  %gc.anchor = alloca i8, align 1
  call void @__visuall_gc_init(ptr nonnull %gc.anchor)
  br label %while.cond4.preheader.i

while.cond4.preheader.i:                          ; preds = %while.end6.i, %entry
  %total.027.i = phi i64 [ 0, %entry ], [ %add.i.9, %while.end6.i ]
  %i.026.i = phi i64 [ 0, %entry ], [ %add16.i, %while.end6.i ]
  br label %while.body5.i

while.body5.i:                                    ; preds = %while.body5.i, %while.cond4.preheader.i
  %j.025.i = phi i64 [ 0, %while.cond4.preheader.i ], [ %add14.i.9, %while.body5.i ]
  %total.124.i = phi i64 [ %total.027.i, %while.cond4.preheader.i ], [ %add.i.9, %while.body5.i ]
  %mul.i = mul nuw nsw i64 %j.025.i, %i.026.i
  %mod.i = urem i64 %mul.i, 97
  %add.i = add i64 %mod.i, %total.124.i
  %add14.i = or disjoint i64 %j.025.i, 1
  %mul.i.1 = mul nuw nsw i64 %add14.i, %i.026.i
  %mod.i.1 = urem i64 %mul.i.1, 97
  %add.i.1 = add i64 %mod.i.1, %add.i
  %add14.i.1 = add nuw nsw i64 %j.025.i, 2
  %mul.i.2 = mul nuw nsw i64 %add14.i.1, %i.026.i
  %mod.i.2 = urem i64 %mul.i.2, 97
  %add.i.2 = add i64 %mod.i.2, %add.i.1
  %add14.i.2 = add nuw nsw i64 %j.025.i, 3
  %mul.i.3 = mul nuw nsw i64 %add14.i.2, %i.026.i
  %mod.i.3 = urem i64 %mul.i.3, 97
  %add.i.3 = add i64 %mod.i.3, %add.i.2
  %add14.i.3 = add nuw nsw i64 %j.025.i, 4
  %mul.i.4 = mul nuw nsw i64 %add14.i.3, %i.026.i
  %mod.i.4 = urem i64 %mul.i.4, 97
  %add.i.4 = add i64 %mod.i.4, %add.i.3
  %add14.i.4 = add nuw nsw i64 %j.025.i, 5
  %mul.i.5 = mul nuw nsw i64 %add14.i.4, %i.026.i
  %mod.i.5 = urem i64 %mul.i.5, 97
  %add.i.5 = add i64 %mod.i.5, %add.i.4
  %add14.i.5 = add nuw nsw i64 %j.025.i, 6
  %mul.i.6 = mul nuw nsw i64 %add14.i.5, %i.026.i
  %mod.i.6 = urem i64 %mul.i.6, 97
  %add.i.6 = add i64 %mod.i.6, %add.i.5
  %add14.i.6 = add nuw nsw i64 %j.025.i, 7
  %mul.i.7 = mul nuw nsw i64 %add14.i.6, %i.026.i
  %mod.i.7 = urem i64 %mul.i.7, 97
  %add.i.7 = add i64 %mod.i.7, %add.i.6
  %add14.i.7 = add nuw nsw i64 %j.025.i, 8
  %mul.i.8 = mul nuw nsw i64 %add14.i.7, %i.026.i
  %mod.i.8 = urem i64 %mul.i.8, 97
  %add.i.8 = add i64 %mod.i.8, %add.i.7
  %add14.i.8 = add nuw nsw i64 %j.025.i, 9
  %mul.i.9 = mul nuw nsw i64 %add14.i.8, %i.026.i
  %mod.i.9 = urem i64 %mul.i.9, 97
  %add.i.9 = add i64 %mod.i.9, %add.i.8
  %add14.i.9 = add nuw nsw i64 %j.025.i, 10
  %exitcond.not.i.9 = icmp eq i64 %add14.i.9, 2000
  br i1 %exitcond.not.i.9, label %while.end6.i, label %while.body5.i

while.end6.i:                                     ; preds = %while.body5.i
  %add16.i = add nuw nsw i64 %i.026.i, 1
  %exitcond28.not.i = icmp eq i64 %add16.i, 2000
  br i1 %exitcond28.not.i, label %nested_loops.exit, label %while.cond4.preheader.i

nested_loops.exit:                                ; preds = %while.end6.i
  call void @__visuall_print_int(i64 %add.i.9)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}
