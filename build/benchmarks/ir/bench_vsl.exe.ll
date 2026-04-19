; ModuleID = 'bench.vsl'
source_filename = "bench.vsl"

declare void @__visuall_print_int(i64) local_unnamed_addr

declare void @__visuall_print_newline() local_unnamed_addr

declare i64 @__visuall_list_get(ptr, i64) local_unnamed_addr

declare i64 @__visuall_list_len(ptr) local_unnamed_addr

declare ptr @__visuall_range(i64, i64, i64) local_unnamed_addr

declare void @__visuall_gc_shutdown() local_unnamed_addr

define i64 @fibonacci(i64 %n) local_unnamed_addr {
entry:
  %le = icmp slt i64 %n, 2
  br i1 %le, label %common.ret, label %if.end

common.ret:                                       ; preds = %for.body, %if.end, %entry
  %common.ret.op = phi i64 [ %n, %entry ], [ 1, %if.end ], [ %add8, %for.body ]
  ret i64 %common.ret.op

if.end:                                           ; preds = %entry
  %add = add nuw i64 %n, 1
  %range.result = tail call ptr @__visuall_range(i64 2, i64 %add, i64 1)
  %list.len = tail call i64 @__visuall_list_len(ptr %range.result)
  %for.cmp18 = icmp sgt i64 %list.len, 0
  br i1 %for.cmp18, label %for.body, label %common.ret

for.body:                                         ; preds = %if.end, %for.body
  %__for_idx.021 = phi i64 [ %inc, %for.body ], [ 0, %if.end ]
  %a.020 = phi i64 [ %b.019, %for.body ], [ 0, %if.end ]
  %b.019 = phi i64 [ %add8, %for.body ], [ 1, %if.end ]
  %elem = tail call i64 @__visuall_list_get(ptr %range.result, i64 %__for_idx.021)
  %add8 = add i64 %a.020, %b.019
  %inc = add nuw nsw i64 %__for_idx.021, 1
  %for.cmp = icmp slt i64 %inc, %list.len
  br i1 %for.cmp, label %for.body, label %common.ret
}

define noundef i32 @main() local_unnamed_addr {
entry:
  %gc.anchor = alloca i8, align 1
  call void @__visuall_gc_init.1(ptr nonnull %gc.anchor)
  %range.result = call ptr @__visuall_range(i64 0, i64 1000000, i64 1)
  %list.len = call i64 @__visuall_list_len(ptr %range.result)
  %for.cmp7 = icmp sgt i64 %list.len, 0
  br i1 %for.cmp7, label %for.body, label %for.end

for.body:                                         ; preds = %entry, %fibonacci.exit
  %total.09 = phi i64 [ %add, %fibonacci.exit ], [ 0, %entry ]
  %__for_idx.08 = phi i64 [ %inc, %fibonacci.exit ], [ 0, %entry ]
  %elem = call i64 @__visuall_list_get(ptr %range.result, i64 %__for_idx.08)
  %range.result.i = call ptr @__visuall_range(i64 2, i64 31, i64 1)
  %list.len.i = call i64 @__visuall_list_len(ptr %range.result.i)
  %for.cmp18.i = icmp sgt i64 %list.len.i, 0
  br i1 %for.cmp18.i, label %for.body.i, label %fibonacci.exit

for.body.i:                                       ; preds = %for.body, %for.body.i
  %__for_idx.021.i = phi i64 [ %inc.i, %for.body.i ], [ 0, %for.body ]
  %a.020.i = phi i64 [ %b.019.i, %for.body.i ], [ 0, %for.body ]
  %b.019.i = phi i64 [ %add8.i, %for.body.i ], [ 1, %for.body ]
  %elem.i = call i64 @__visuall_list_get(ptr %range.result.i, i64 %__for_idx.021.i)
  %add8.i = add i64 %b.019.i, %a.020.i
  %inc.i = add nuw nsw i64 %__for_idx.021.i, 1
  %for.cmp.i = icmp slt i64 %inc.i, %list.len.i
  br i1 %for.cmp.i, label %for.body.i, label %fibonacci.exit

fibonacci.exit:                                   ; preds = %for.body.i, %for.body
  %common.ret.op.i = phi i64 [ 1, %for.body ], [ %add8.i, %for.body.i ]
  %add = add i64 %common.ret.op.i, %total.09
  %inc = add nuw nsw i64 %__for_idx.08, 1
  %for.cmp = icmp slt i64 %inc, %list.len
  br i1 %for.cmp, label %for.body, label %for.end

for.end:                                          ; preds = %fibonacci.exit, %entry
  %total.0.lcssa = phi i64 [ 0, %entry ], [ %add, %fibonacci.exit ]
  call void @__visuall_print_int(i64 %total.0.lcssa)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}

declare void @__visuall_gc_init.1(ptr) local_unnamed_addr
