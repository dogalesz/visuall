; ModuleID = 'bench2.vsl'
source_filename = "bench2.vsl"

declare void @__visuall_print_int(i64) local_unnamed_addr

declare void @__visuall_print_newline() local_unnamed_addr

declare void @__visuall_gc_shutdown() local_unnamed_addr

; Function Attrs: nofree norecurse nosync nounwind memory(none)
define i64 @fibonacci(i64 %n) local_unnamed_addr #0 {
entry:
  %le = icmp slt i64 %n, 2
  br i1 %le, label %common.ret, label %while.body

common.ret:                                       ; preds = %while.body, %entry
  %common.ret.op = phi i64 [ %n, %entry ], [ %add, %while.body ]
  ret i64 %common.ret.op

while.body:                                       ; preds = %entry, %while.body
  %i.021 = phi i64 [ %add12, %while.body ], [ 2, %entry ]
  %a.020 = phi i64 [ %b.019, %while.body ], [ 0, %entry ]
  %b.019 = phi i64 [ %add, %while.body ], [ 1, %entry ]
  %add = add i64 %a.020, %b.019
  %add12 = add i64 %i.021, 1
  %le6.not = icmp sgt i64 %add12, %n
  br i1 %le6.not, label %common.ret, label %while.body
}

define noundef i32 @main() local_unnamed_addr {
entry:
  %gc.anchor = alloca i8, align 1
  call void @__visuall_gc_init.1(ptr nonnull %gc.anchor)
  call void @__visuall_print_int(i64 832040000000)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}

declare void @__visuall_gc_init.1(ptr) local_unnamed_addr

attributes #0 = { nofree norecurse nosync nounwind memory(none) }
