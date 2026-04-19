; ModuleID = 'bench_pi.vsl'
source_filename = "bench_pi.vsl"
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
  %i.016.i = phi i64 [ 0, %entry ], [ %add9.i.9, %while.body.i ]
  %pi.015.i = phi double [ 0.000000e+00, %entry ], [ %fadd.i.9, %while.body.i ]
  %mul.i = shl nuw nsw i64 %i.016.i, 1
  %add.i = or disjoint i64 %mul.i, 1
  %tofloat.i = uitofp nneg i64 %add.i to double
  %fdiv.i = fdiv double 1.000000e+00, %tofloat.i
  %fadd.i = fadd double %pi.015.i, %fdiv.i
  %add9.i = shl nuw i64 %i.016.i, 1
  %add.i.1 = or disjoint i64 %add9.i, 3
  %tofloat.i.1 = uitofp nneg i64 %add.i.1 to double
  %fdiv.i.1 = fdiv double -1.000000e+00, %tofloat.i.1
  %fadd.i.1 = fadd double %fadd.i, %fdiv.i.1
  %add9.i.1 = shl nuw i64 %i.016.i, 1
  %add.i.2 = add i64 %add9.i.1, 5
  %tofloat.i.2 = uitofp nneg i64 %add.i.2 to double
  %fdiv.i.2 = fdiv double 1.000000e+00, %tofloat.i.2
  %fadd.i.2 = fadd double %fadd.i.1, %fdiv.i.2
  %add9.i.2 = shl nuw i64 %i.016.i, 1
  %add.i.3 = add i64 %add9.i.2, 7
  %tofloat.i.3 = uitofp nneg i64 %add.i.3 to double
  %fdiv.i.3 = fdiv double -1.000000e+00, %tofloat.i.3
  %fadd.i.3 = fadd double %fadd.i.2, %fdiv.i.3
  %add9.i.3 = shl nuw i64 %i.016.i, 1
  %add.i.4 = add i64 %add9.i.3, 9
  %tofloat.i.4 = uitofp nneg i64 %add.i.4 to double
  %fdiv.i.4 = fdiv double 1.000000e+00, %tofloat.i.4
  %fadd.i.4 = fadd double %fadd.i.3, %fdiv.i.4
  %add9.i.4 = shl nuw i64 %i.016.i, 1
  %add.i.5 = add i64 %add9.i.4, 11
  %tofloat.i.5 = uitofp nneg i64 %add.i.5 to double
  %fdiv.i.5 = fdiv double -1.000000e+00, %tofloat.i.5
  %fadd.i.5 = fadd double %fadd.i.4, %fdiv.i.5
  %add9.i.5 = shl nuw i64 %i.016.i, 1
  %add.i.6 = add i64 %add9.i.5, 13
  %tofloat.i.6 = uitofp nneg i64 %add.i.6 to double
  %fdiv.i.6 = fdiv double 1.000000e+00, %tofloat.i.6
  %fadd.i.6 = fadd double %fadd.i.5, %fdiv.i.6
  %add9.i.6 = shl nuw i64 %i.016.i, 1
  %add.i.7 = add i64 %add9.i.6, 15
  %tofloat.i.7 = uitofp nneg i64 %add.i.7 to double
  %fdiv.i.7 = fdiv double -1.000000e+00, %tofloat.i.7
  %fadd.i.7 = fadd double %fadd.i.6, %fdiv.i.7
  %add9.i.7 = shl nuw i64 %i.016.i, 1
  %add.i.8 = add i64 %add9.i.7, 17
  %tofloat.i.8 = uitofp nneg i64 %add.i.8 to double
  %fdiv.i.8 = fdiv double 1.000000e+00, %tofloat.i.8
  %fadd.i.8 = fadd double %fadd.i.7, %fdiv.i.8
  %add9.i.8 = shl nuw i64 %i.016.i, 1
  %add.i.9 = add i64 %add9.i.8, 19
  %tofloat.i.9 = uitofp nneg i64 %add.i.9 to double
  %fdiv.i.9 = fdiv double -1.000000e+00, %tofloat.i.9
  %fadd.i.9 = fadd double %fadd.i.8, %fdiv.i.9
  %add9.i.9 = add nuw nsw i64 %i.016.i, 10
  %exitcond.not.i.9 = icmp eq i64 %add9.i.9, 10000000
  br i1 %exitcond.not.i.9, label %compute_pi.exit, label %while.body.i

compute_pi.exit:                                  ; preds = %while.body.i
  %fmul11.i = fmul double %fadd.i.9, 4.000000e+00
  call void @__visuall_print_float(double %fmul11.i)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}
