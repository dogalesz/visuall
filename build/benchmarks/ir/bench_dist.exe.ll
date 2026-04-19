; ModuleID = 'bench_dist.vsl'
source_filename = "bench_dist.vsl"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-w64-windows-gnu"

declare void @__visuall_print_float(double) local_unnamed_addr

declare void @__visuall_print_newline() local_unnamed_addr

declare void @__visuall_gc_init(ptr) local_unnamed_addr

declare void @__visuall_gc_shutdown() local_unnamed_addr

define noundef i32 @main() local_unnamed_addr {
entry:
  %gc.anchor = alloca i8, align 1
  call void @__visuall_gc_init(ptr nonnull %gc.anchor)
  br label %while.body.i

while.body.i:                                     ; preds = %while.body.i, %entry
  %i.024.i = phi i64 [ 0, %entry ], [ %add.i.3, %while.body.i ]
  %total_dist.023.i = phi double [ 0.000000e+00, %entry ], [ %fadd15.i.3, %while.body.i ]
  %mod.lhs.trunc.i = trunc nuw i64 %i.024.i to i32
  %mod21.i = urem i32 %mod.lhs.trunc.i, 100
  %mod.zext.i = zext nneg i32 %mod21.i to i64
  %0 = add nsw i64 %mod.zext.i, -50
  %mod622.i = urem i32 %mod.lhs.trunc.i, 73
  %tofloat7.i = uitofp nneg i32 %mod622.i to double
  %fsub8.i = fadd double %tofloat7.i, -3.650000e+01
  %1 = mul nsw i64 %0, %0
  %fmul.i = uitofp nneg i64 %1 to double
  %fadd.i = fadd double %total_dist.023.i, %fmul.i
  %fmul14.i = fmul double %fsub8.i, %fsub8.i
  %fadd15.i = fadd double %fmul14.i, %fadd.i
  %2 = trunc i64 %i.024.i to i32
  %mod.lhs.trunc.i.1 = or disjoint i32 %2, 1
  %mod21.i.1 = urem i32 %mod.lhs.trunc.i.1, 100
  %mod.zext.i.1 = zext nneg i32 %mod21.i.1 to i64
  %3 = add nsw i64 %mod.zext.i.1, -50
  %mod622.i.1 = urem i32 %mod.lhs.trunc.i.1, 73
  %tofloat7.i.1 = uitofp nneg i32 %mod622.i.1 to double
  %fsub8.i.1 = fadd double %tofloat7.i.1, -3.650000e+01
  %4 = mul nsw i64 %3, %3
  %fmul.i.1 = uitofp nneg i64 %4 to double
  %fadd.i.1 = fadd double %fadd15.i, %fmul.i.1
  %fmul14.i.1 = fmul double %fsub8.i.1, %fsub8.i.1
  %fadd15.i.1 = fadd double %fmul14.i.1, %fadd.i.1
  %5 = trunc i64 %i.024.i to i32
  %mod.lhs.trunc.i.2 = or disjoint i32 %5, 2
  %mod21.i.2 = urem i32 %mod.lhs.trunc.i.2, 100
  %mod.zext.i.2 = zext nneg i32 %mod21.i.2 to i64
  %6 = add nsw i64 %mod.zext.i.2, -50
  %fsub.i.2 = sitofp i64 %6 to double
  %mod622.i.2 = urem i32 %mod.lhs.trunc.i.2, 73
  %tofloat7.i.2 = uitofp nneg i32 %mod622.i.2 to double
  %fsub8.i.2 = fadd double %tofloat7.i.2, -3.650000e+01
  %fmul.i.2 = fmul double %fsub.i.2, %fsub.i.2
  %fadd.i.2 = fadd double %fadd15.i.1, %fmul.i.2
  %fmul14.i.2 = fmul double %fsub8.i.2, %fsub8.i.2
  %fadd15.i.2 = fadd double %fmul14.i.2, %fadd.i.2
  %7 = trunc i64 %i.024.i to i32
  %mod.lhs.trunc.i.3 = or disjoint i32 %7, 3
  %mod21.i.3 = urem i32 %mod.lhs.trunc.i.3, 100
  %mod.zext.i.3 = zext nneg i32 %mod21.i.3 to i64
  %8 = add nsw i64 %mod.zext.i.3, -50
  %mod622.i.3 = urem i32 %mod.lhs.trunc.i.3, 73
  %tofloat7.i.3 = uitofp nneg i32 %mod622.i.3 to double
  %fsub8.i.3 = fadd double %tofloat7.i.3, -3.650000e+01
  %9 = mul nsw i64 %8, %8
  %fmul.i.3 = uitofp nneg i64 %9 to double
  %fadd.i.3 = fadd double %fadd15.i.2, %fmul.i.3
  %fmul14.i.3 = fmul double %fsub8.i.3, %fsub8.i.3
  %fadd15.i.3 = fadd double %fmul14.i.3, %fadd.i.3
  %add.i.3 = add nuw nsw i64 %i.024.i, 4
  %exitcond.not.i.3 = icmp eq i64 %add.i.3, 1000000
  br i1 %exitcond.not.i.3, label %dist_bench.exit, label %while.body.i

dist_bench.exit:                                  ; preds = %while.body.i
  call void @__visuall_print_float(double %fadd15.i.3)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}
