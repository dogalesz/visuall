; ModuleID = 'bench_fseq.vsl'
source_filename = "bench_fseq.vsl"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-w64-windows-gnu"

@fstr.lit.1 = private unnamed_addr constant [5 x i8] c"key_\00", align 1

declare void @__visuall_print_int(i64) local_unnamed_addr

declare void @__visuall_print_newline() local_unnamed_addr

declare ptr @__visuall_fstring_build(ptr, i32) local_unnamed_addr

declare i64 @__visuall_str_eq(ptr, ptr) local_unnamed_addr

declare void @__visuall_gc_init(ptr) local_unnamed_addr

declare void @__visuall_gc_shutdown() local_unnamed_addr

define noundef i32 @main() local_unnamed_addr {
entry:
  %fstr.parts6.i = alloca [2 x ptr], align 8
  %fstr.parts.i = alloca [2 x ptr], align 8
  %gc.anchor = alloca i8, align 1
  call void @__visuall_gc_init(ptr nonnull %gc.anchor)
  call void @llvm.lifetime.start.p0(i64 16, ptr nonnull %fstr.parts6.i)
  call void @llvm.lifetime.start.p0(i64 16, ptr nonnull %fstr.parts.i)
  %fstr.gep5.i = getelementptr inbounds nuw i8, ptr %fstr.parts.i, i64 8
  %fstr.gep12.i = getelementptr inbounds nuw i8, ptr %fstr.parts6.i, i64 8
  br label %while.body.i

while.body.i:                                     ; preds = %while.body.i, %entry
  %i.026.i = phi i64 [ 0, %entry ], [ %add19.i, %while.body.i ]
  %matches.025.i = phi i64 [ 0, %entry ], [ %spec.select.i, %while.body.i ]
  store ptr @fstr.lit.1, ptr %fstr.parts.i, align 8
  %fstr.shl.i = shl nuw nsw i64 %i.026.i, 1
  %fstr.tag.i = or disjoint i64 %fstr.shl.i, 1
  %fstr.tagptr.i = inttoptr i64 %fstr.tag.i to ptr
  store ptr %fstr.tagptr.i, ptr %fstr.gep5.i, align 8
  %fstr.result.i = call ptr @__visuall_fstring_build(ptr nonnull %fstr.parts.i, i32 2)
  store ptr @fstr.lit.1, ptr %fstr.parts6.i, align 8
  store ptr %fstr.tagptr.i, ptr %fstr.gep12.i, align 8
  %fstr.result14.i = call ptr @__visuall_fstring_build(ptr nonnull %fstr.parts6.i, i32 2)
  %str.eq.i = call i64 @__visuall_str_eq(ptr %fstr.result.i, ptr %fstr.result14.i)
  %str.cmp.not.i = icmp ne i64 %str.eq.i, 0
  %add.i = zext i1 %str.cmp.not.i to i64
  %spec.select.i = add i64 %matches.025.i, %add.i
  %add19.i = add nuw nsw i64 %i.026.i, 1
  %exitcond.not.i = icmp eq i64 %add19.i, 500000
  br i1 %exitcond.not.i, label %bench.exit, label %while.body.i

bench.exit:                                       ; preds = %while.body.i
  call void @llvm.lifetime.end.p0(i64 16, ptr nonnull %fstr.parts6.i)
  call void @llvm.lifetime.end.p0(i64 16, ptr nonnull %fstr.parts.i)
  call void @__visuall_print_int(i64 %spec.select.i)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #0

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #0

attributes #0 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
