; ModuleID = 'bench_tree.vsl'
source_filename = "bench_tree.vsl"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-w64-windows-gnu"

declare void @__visuall_print_int(i64) local_unnamed_addr

declare void @__visuall_print_newline() local_unnamed_addr

declare void @__visuall_gc_init(ptr) local_unnamed_addr

declare void @__visuall_gc_shutdown() local_unnamed_addr

; Function Attrs: nofree nosync nounwind memory(none)
define internal fastcc i64 @build_tree_sum(i64 %depth, i64 %val) unnamed_addr #0 {
entry:
  %le22 = icmp slt i64 %depth, 1
  br i1 %le22, label %common.ret, label %if.end

common.ret:                                       ; preds = %if.end, %entry
  %accumulator.tr.lcssa = phi i64 [ 0, %entry ], [ %add16, %if.end ]
  %val.tr.lcssa = phi i64 [ %val, %entry ], [ %add, %if.end ]
  %accumulator.ret.tr = add i64 %val.tr.lcssa, %accumulator.tr.lcssa
  ret i64 %accumulator.ret.tr

if.end:                                           ; preds = %entry, %if.end
  %val.tr25 = phi i64 [ %add, %if.end ], [ %val, %entry ]
  %depth.tr24 = phi i64 [ %sub, %if.end ], [ %depth, %entry ]
  %accumulator.tr23 = phi i64 [ %add16, %if.end ], [ 0, %entry ]
  %sub = add nsw i64 %depth.tr24, -1
  %mul = shl i64 %val.tr25, 1
  %call = tail call fastcc i64 @build_tree_sum(i64 %sub, i64 %mul)
  %add = or disjoint i64 %mul, 1
  %add14 = add i64 %val.tr25, %accumulator.tr23
  %add16 = add i64 %add14, %call
  %le = icmp samesign ult i64 %depth.tr24, 2
  br i1 %le, label %common.ret, label %if.end
}

define noundef i32 @main() local_unnamed_addr {
entry:
  %gc.anchor = alloca i8, align 1
  call void @__visuall_gc_init(ptr nonnull %gc.anchor)
  %call = call fastcc i64 @build_tree_sum(i64 22, i64 1)
  call void @__visuall_print_int(i64 %call)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}

attributes #0 = { nofree nosync nounwind memory(none) }
