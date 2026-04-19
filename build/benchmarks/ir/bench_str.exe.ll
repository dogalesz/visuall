; ModuleID = 'bench_str.vsl'
source_filename = "bench_str.vsl"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-w64-windows-gnu"

@fstr.lit = private unnamed_addr constant [6 x i8] c"item_\00", align 1
@fstr.lit.1 = private unnamed_addr constant [8 x i8] c"_value_\00", align 1

declare void @__visuall_print_int(i64) local_unnamed_addr

declare void @__visuall_print_newline() local_unnamed_addr

declare ptr @__visuall_fstring_build(ptr, i32) local_unnamed_addr

declare i64 @__visuall_str_len(ptr) local_unnamed_addr

declare void @__visuall_gc_init(ptr) local_unnamed_addr

declare void @__visuall_gc_shutdown() local_unnamed_addr

define noundef i32 @main() local_unnamed_addr {
entry:
  %fstr.parts.i = alloca [4 x ptr], align 8
  %gc.anchor = alloca i8, align 1
  call void @__visuall_gc_init(ptr nonnull %gc.anchor)
  call void @llvm.lifetime.start.p0(i64 32, ptr nonnull %fstr.parts.i)
  %fstr.gep5.i = getelementptr inbounds nuw i8, ptr %fstr.parts.i, i64 8
  %fstr.gep6.i = getelementptr inbounds nuw i8, ptr %fstr.parts.i, i64 16
  %fstr.gep11.i = getelementptr inbounds nuw i8, ptr %fstr.parts.i, i64 24
  br label %while.body.i

while.body.i:                                     ; preds = %while.body.i, %entry
  %i.022.i = phi i64 [ 0, %entry ], [ %add15.i, %while.body.i ]
  %total_len.021.i = phi i64 [ 0, %entry ], [ %add.i, %while.body.i ]
  store ptr @fstr.lit, ptr %fstr.parts.i, align 8
  %fstr.shl.i = shl nuw nsw i64 %i.022.i, 1
  %fstr.tag.i = or disjoint i64 %fstr.shl.i, 1
  %fstr.tagptr.i = inttoptr i64 %fstr.tag.i to ptr
  store ptr %fstr.tagptr.i, ptr %fstr.gep5.i, align 8
  store ptr @fstr.lit.1, ptr %fstr.gep6.i, align 8
  %fstr.shl8.i = mul nuw nsw i64 %i.022.i, 14
  %fstr.tag9.i = or disjoint i64 %fstr.shl8.i, 1
  %fstr.tagptr10.i = inttoptr i64 %fstr.tag9.i to ptr
  store ptr %fstr.tagptr10.i, ptr %fstr.gep11.i, align 8
  %fstr.result.i = call ptr @__visuall_fstring_build(ptr nonnull %fstr.parts.i, i32 4)
  %len.i = call i64 @__visuall_str_len(ptr %fstr.result.i)
  %add.i = add i64 %len.i, %total_len.021.i
  %add15.i = add nuw nsw i64 %i.022.i, 1
  %exitcond.not.i = icmp eq i64 %add15.i, 200000
  br i1 %exitcond.not.i, label %string_build.exit, label %while.body.i

string_build.exit:                                ; preds = %while.body.i
  call void @llvm.lifetime.end.p0(i64 32, ptr nonnull %fstr.parts.i)
  call void @__visuall_print_int(i64 %add.i)
  call void @__visuall_print_newline()
  call void @__visuall_gc_shutdown()
  ret i32 0
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #0

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #0

attributes #0 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
