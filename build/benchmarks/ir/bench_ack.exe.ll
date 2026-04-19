; ModuleID = 'bench_ack.vsl'
source_filename = "bench_ack.vsl"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-w64-windows-gnu"

declare void @__visuall_print_int(i64) local_unnamed_addr

declare void @__visuall_print_newline() local_unnamed_addr

declare void @__visuall_gc_init(ptr) local_unnamed_addr

declare void @__visuall_gc_shutdown() local_unnamed_addr

; Function Attrs: nofree nosync nounwind memory(none)
define internal fastcc i64 @ackermann(i64 range(i64 0, 4) %m, i64 %n) unnamed_addr #0 {
entry:
  %eq22 = icmp eq i64 %m, 0
  br i1 %eq22, label %if.then, label %if.end

if.then:                                          ; preds = %tailrecurse.backedge, %entry
  %n.tr.lcssa = phi i64 [ %n, %entry ], [ %n.tr.be, %tailrecurse.backedge ]
  %add = add i64 %n.tr.lcssa, 1
  ret i64 %add

if.end:                                           ; preds = %entry, %tailrecurse.backedge
  %n.tr24 = phi i64 [ %n.tr.be, %tailrecurse.backedge ], [ %n, %entry ]
  %m.tr23 = phi i64 [ %m.tr.be, %tailrecurse.backedge ], [ %m, %entry ]
  %eq6 = icmp eq i64 %n.tr24, 0
  br i1 %eq6, label %tailrecurse.backedge, label %if.end9

tailrecurse.backedge:                             ; preds = %if.end, %if.end9
  %n.tr.be = phi i64 [ %call15, %if.end9 ], [ 1, %if.end ]
  %m.tr.be = add nsw i64 %m.tr23, -1
  %eq = icmp eq i64 %m.tr.be, 0
  br i1 %eq, label %if.then, label %if.end

if.end9:                                          ; preds = %if.end
  %sub14 = add i64 %n.tr24, -1
  %call15 = tail call fastcc i64 @ackermann(i64 %m.tr23, i64 %sub14)
  br label %tailrecurse.backedge
}

define noundef i32 @main() local_unnamed_addr {
entry:
  %gc.anchor = alloca i8, align 1
  call void @__visuall_gc_init(ptr nonnull %gc.anchor)
  %call = call fastcc i64 @ackermann(i64 3, i64 11)
  call void @__visuall_print_int(i64 %call)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}

attributes #0 = { nofree nosync nounwind memory(none) }
