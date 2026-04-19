; ModuleID = 'bench_gcd.vsl'
source_filename = "bench_gcd.vsl"
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
  br label %while.cond5.preheader.i

while.cond5.preheader.i:                          ; preds = %while.end7.i, %entry
  %indvar = phi i64 [ %indvar.next, %while.end7.i ], [ 0, %entry ]
  %total.029.i = phi i64 [ %add.i.lcssa, %while.end7.i ], [ 0, %entry ]
  %i.028.i = phi i64 [ %add17.i, %while.end7.i ], [ 1, %entry ]
  %0 = sub i64 0, %indvar
  %xtraiter = and i64 %0, 3
  %lcmp.mod.not = icmp eq i64 %xtraiter, 0
  br i1 %lcmp.mod.not, label %while.body.i.preheader.i.prol.loopexit, label %while.body.i.preheader.i.prol

while.body.i.preheader.i.prol:                    ; preds = %while.cond5.preheader.i, %gcd.exit.i.prol
  %j.027.i.prol = phi i64 [ %add15.i.prol, %gcd.exit.i.prol ], [ %i.028.i, %while.cond5.preheader.i ]
  %total.126.i.prol = phi i64 [ %add.i.prol, %gcd.exit.i.prol ], [ %total.029.i, %while.cond5.preheader.i ]
  %prol.iter = phi i64 [ %prol.iter.next, %gcd.exit.i.prol ], [ 0, %while.cond5.preheader.i ]
  br label %while.body.i.i.prol

while.body.i.i.prol:                              ; preds = %while.body.i.i.prol, %while.body.i.preheader.i.prol
  %a1.014.i.i.prol = phi i64 [ %b2.013.i.i.prol, %while.body.i.i.prol ], [ %i.028.i, %while.body.i.preheader.i.prol ]
  %b2.013.i.i.prol = phi i64 [ %mod.i.i.prol, %while.body.i.i.prol ], [ %j.027.i.prol, %while.body.i.preheader.i.prol ]
  %mod.i.i.prol = srem i64 %a1.014.i.i.prol, %b2.013.i.i.prol
  %ne.not.i.i.prol = icmp eq i64 %mod.i.i.prol, 0
  br i1 %ne.not.i.i.prol, label %gcd.exit.i.prol, label %while.body.i.i.prol

gcd.exit.i.prol:                                  ; preds = %while.body.i.i.prol
  %add.i.prol = add i64 %b2.013.i.i.prol, %total.126.i.prol
  %add15.i.prol = add nuw nsw i64 %j.027.i.prol, 1
  %prol.iter.next = add i64 %prol.iter, 1
  %prol.iter.cmp.not = icmp eq i64 %prol.iter.next, %xtraiter
  br i1 %prol.iter.cmp.not, label %while.body.i.preheader.i.prol.loopexit, label %while.body.i.preheader.i.prol, !llvm.loop !0

while.body.i.preheader.i.prol.loopexit:           ; preds = %gcd.exit.i.prol, %while.cond5.preheader.i
  %add.i.lcssa.unr = phi i64 [ poison, %while.cond5.preheader.i ], [ %add.i.prol, %gcd.exit.i.prol ]
  %j.027.i.unr = phi i64 [ %i.028.i, %while.cond5.preheader.i ], [ %add15.i.prol, %gcd.exit.i.prol ]
  %total.126.i.unr = phi i64 [ %total.029.i, %while.cond5.preheader.i ], [ %add.i.prol, %gcd.exit.i.prol ]
  %1 = add i64 %indvar, -1997
  %2 = icmp ult i64 %1, 3
  br i1 %2, label %while.end7.i, label %while.body.i.preheader.i

while.body.i.preheader.i:                         ; preds = %while.body.i.preheader.i.prol.loopexit, %gcd.exit.i.3
  %j.027.i = phi i64 [ %add15.i.3, %gcd.exit.i.3 ], [ %j.027.i.unr, %while.body.i.preheader.i.prol.loopexit ]
  %total.126.i = phi i64 [ %add.i.3, %gcd.exit.i.3 ], [ %total.126.i.unr, %while.body.i.preheader.i.prol.loopexit ]
  br label %while.body.i.i

while.body.i.i:                                   ; preds = %while.body.i.i, %while.body.i.preheader.i
  %a1.014.i.i = phi i64 [ %b2.013.i.i, %while.body.i.i ], [ %i.028.i, %while.body.i.preheader.i ]
  %b2.013.i.i = phi i64 [ %mod.i.i, %while.body.i.i ], [ %j.027.i, %while.body.i.preheader.i ]
  %mod.i.i = srem i64 %a1.014.i.i, %b2.013.i.i
  %ne.not.i.i = icmp eq i64 %mod.i.i, 0
  br i1 %ne.not.i.i, label %gcd.exit.i, label %while.body.i.i

gcd.exit.i:                                       ; preds = %while.body.i.i
  %add.i = add i64 %b2.013.i.i, %total.126.i
  %add15.i = add nuw nsw i64 %j.027.i, 1
  br label %while.body.i.i.1

while.body.i.i.1:                                 ; preds = %while.body.i.i.1, %gcd.exit.i
  %a1.014.i.i.1 = phi i64 [ %b2.013.i.i.1, %while.body.i.i.1 ], [ %i.028.i, %gcd.exit.i ]
  %b2.013.i.i.1 = phi i64 [ %mod.i.i.1, %while.body.i.i.1 ], [ %add15.i, %gcd.exit.i ]
  %mod.i.i.1 = srem i64 %a1.014.i.i.1, %b2.013.i.i.1
  %ne.not.i.i.1 = icmp eq i64 %mod.i.i.1, 0
  br i1 %ne.not.i.i.1, label %gcd.exit.i.1, label %while.body.i.i.1

gcd.exit.i.1:                                     ; preds = %while.body.i.i.1
  %add.i.1 = add i64 %b2.013.i.i.1, %add.i
  %add15.i.1 = add nuw nsw i64 %j.027.i, 2
  br label %while.body.i.i.2

while.body.i.i.2:                                 ; preds = %while.body.i.i.2, %gcd.exit.i.1
  %a1.014.i.i.2 = phi i64 [ %b2.013.i.i.2, %while.body.i.i.2 ], [ %i.028.i, %gcd.exit.i.1 ]
  %b2.013.i.i.2 = phi i64 [ %mod.i.i.2, %while.body.i.i.2 ], [ %add15.i.1, %gcd.exit.i.1 ]
  %mod.i.i.2 = srem i64 %a1.014.i.i.2, %b2.013.i.i.2
  %ne.not.i.i.2 = icmp eq i64 %mod.i.i.2, 0
  br i1 %ne.not.i.i.2, label %gcd.exit.i.2, label %while.body.i.i.2

gcd.exit.i.2:                                     ; preds = %while.body.i.i.2
  %add.i.2 = add i64 %b2.013.i.i.2, %add.i.1
  %add15.i.2 = add nuw nsw i64 %j.027.i, 3
  br label %while.body.i.i.3

while.body.i.i.3:                                 ; preds = %while.body.i.i.3, %gcd.exit.i.2
  %a1.014.i.i.3 = phi i64 [ %b2.013.i.i.3, %while.body.i.i.3 ], [ %i.028.i, %gcd.exit.i.2 ]
  %b2.013.i.i.3 = phi i64 [ %mod.i.i.3, %while.body.i.i.3 ], [ %add15.i.2, %gcd.exit.i.2 ]
  %mod.i.i.3 = srem i64 %a1.014.i.i.3, %b2.013.i.i.3
  %ne.not.i.i.3 = icmp eq i64 %mod.i.i.3, 0
  br i1 %ne.not.i.i.3, label %gcd.exit.i.3, label %while.body.i.i.3

gcd.exit.i.3:                                     ; preds = %while.body.i.i.3
  %add.i.3 = add i64 %b2.013.i.i.3, %add.i.2
  %add15.i.3 = add nuw nsw i64 %j.027.i, 4
  %exitcond.not.i.3 = icmp eq i64 %add15.i.3, 2001
  br i1 %exitcond.not.i.3, label %while.end7.i, label %while.body.i.preheader.i

while.end7.i:                                     ; preds = %gcd.exit.i.3, %while.body.i.preheader.i.prol.loopexit
  %add.i.lcssa = phi i64 [ %add.i.lcssa.unr, %while.body.i.preheader.i.prol.loopexit ], [ %add.i.3, %gcd.exit.i.3 ]
  %add17.i = add nuw nsw i64 %i.028.i, 1
  %exitcond30.not.i = icmp eq i64 %add17.i, 2001
  %indvar.next = add i64 %indvar, 1
  br i1 %exitcond30.not.i, label %gcd_sum.exit, label %while.cond5.preheader.i

gcd_sum.exit:                                     ; preds = %while.end7.i
  call void @__visuall_print_int(i64 %add.i.lcssa)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}

!0 = distinct !{!0, !1}
!1 = !{!"llvm.loop.unroll.disable"}
