; ModuleID = 'bench_col.vsl'
source_filename = "bench_col.vsl"
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
  br label %while.body

while.body:                                       ; preds = %collatz_steps.exit.3, %entry
  %r.011 = phi i64 [ 0, %entry ], [ %add.3, %collatz_steps.exit.3 ]
  %n.010 = phi i64 [ 1, %entry ], [ %add5.3, %collatz_steps.exit.3 ]
  %ne.not13.i = icmp eq i64 %n.010, 1
  br i1 %ne.not13.i, label %collatz_steps.exit, label %while.body.i

while.body.i:                                     ; preds = %while.body, %while.body.i
  %n1.015.i = phi i64 [ %n1.1.i, %while.body.i ], [ %n.010, %while.body ]
  %steps.014.i = phi i64 [ %add7.i, %while.body.i ], [ 0, %while.body ]
  %0 = and i64 %n1.015.i, 1
  %eq.i = icmp eq i64 %0, 0
  %idiv.i = ashr exact i64 %n1.015.i, 1
  %mul.i = mul i64 %n1.015.i, 3
  %add.i = add i64 %mul.i, 1
  %n1.1.i = select i1 %eq.i, i64 %idiv.i, i64 %add.i
  %add7.i = add i64 %steps.014.i, 1
  %ne.not.i = icmp eq i64 %n1.1.i, 1
  br i1 %ne.not.i, label %collatz_steps.exit, label %while.body.i

collatz_steps.exit:                               ; preds = %while.body.i, %while.body
  %steps.0.lcssa.i = phi i64 [ 0, %while.body ], [ %add7.i, %while.body.i ]
  %add = add i64 %steps.0.lcssa.i, %r.011
  %add5 = add nuw nsw i64 %n.010, 1
  br label %while.body.i.1

while.body.i.1:                                   ; preds = %while.body.i.1, %collatz_steps.exit
  %n1.015.i.1 = phi i64 [ %n1.1.i.1, %while.body.i.1 ], [ %add5, %collatz_steps.exit ]
  %steps.014.i.1 = phi i64 [ %add7.i.1, %while.body.i.1 ], [ 0, %collatz_steps.exit ]
  %1 = and i64 %n1.015.i.1, 1
  %eq.i.1 = icmp eq i64 %1, 0
  %idiv.i.1 = ashr exact i64 %n1.015.i.1, 1
  %mul.i.1 = mul i64 %n1.015.i.1, 3
  %add.i.1 = add i64 %mul.i.1, 1
  %n1.1.i.1 = select i1 %eq.i.1, i64 %idiv.i.1, i64 %add.i.1
  %add7.i.1 = add i64 %steps.014.i.1, 1
  %ne.not.i.1 = icmp eq i64 %n1.1.i.1, 1
  br i1 %ne.not.i.1, label %collatz_steps.exit.1, label %while.body.i.1

collatz_steps.exit.1:                             ; preds = %while.body.i.1
  %add.1 = add i64 %add7.i.1, %add
  %add5.1 = add nuw nsw i64 %n.010, 2
  br label %while.body.i.2

while.body.i.2:                                   ; preds = %while.body.i.2, %collatz_steps.exit.1
  %n1.015.i.2 = phi i64 [ %n1.1.i.2, %while.body.i.2 ], [ %add5.1, %collatz_steps.exit.1 ]
  %steps.014.i.2 = phi i64 [ %add7.i.2, %while.body.i.2 ], [ 0, %collatz_steps.exit.1 ]
  %2 = and i64 %n1.015.i.2, 1
  %eq.i.2 = icmp eq i64 %2, 0
  %idiv.i.2 = ashr exact i64 %n1.015.i.2, 1
  %mul.i.2 = mul i64 %n1.015.i.2, 3
  %add.i.2 = add i64 %mul.i.2, 1
  %n1.1.i.2 = select i1 %eq.i.2, i64 %idiv.i.2, i64 %add.i.2
  %add7.i.2 = add i64 %steps.014.i.2, 1
  %ne.not.i.2 = icmp eq i64 %n1.1.i.2, 1
  br i1 %ne.not.i.2, label %collatz_steps.exit.2, label %while.body.i.2

collatz_steps.exit.2:                             ; preds = %while.body.i.2
  %add.2 = add i64 %add7.i.2, %add.1
  %add5.2 = add nuw nsw i64 %n.010, 3
  br label %while.body.i.3

while.body.i.3:                                   ; preds = %while.body.i.3, %collatz_steps.exit.2
  %n1.015.i.3 = phi i64 [ %n1.1.i.3, %while.body.i.3 ], [ %add5.2, %collatz_steps.exit.2 ]
  %steps.014.i.3 = phi i64 [ %add7.i.3, %while.body.i.3 ], [ 0, %collatz_steps.exit.2 ]
  %3 = and i64 %n1.015.i.3, 1
  %eq.i.3 = icmp eq i64 %3, 0
  %idiv.i.3 = ashr exact i64 %n1.015.i.3, 1
  %mul.i.3 = mul i64 %n1.015.i.3, 3
  %add.i.3 = add i64 %mul.i.3, 1
  %n1.1.i.3 = select i1 %eq.i.3, i64 %idiv.i.3, i64 %add.i.3
  %add7.i.3 = add i64 %steps.014.i.3, 1
  %ne.not.i.3 = icmp eq i64 %n1.1.i.3, 1
  br i1 %ne.not.i.3, label %collatz_steps.exit.3, label %while.body.i.3

collatz_steps.exit.3:                             ; preds = %while.body.i.3
  %add.3 = add i64 %add7.i.3, %add.2
  %add5.3 = add nuw nsw i64 %n.010, 4
  %exitcond.not.3 = icmp eq i64 %add5.3, 100001
  br i1 %exitcond.not.3, label %while.end, label %while.body

while.end:                                        ; preds = %collatz_steps.exit.3
  call void @__visuall_print_int(i64 %add.3)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}
