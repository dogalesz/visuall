; ModuleID = 'test_run3.vsl'
source_filename = "test_run3.vsl"

@str = private unnamed_addr constant [4 x i8] c"big\00", align 1
@fmt_str = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt_int = private unnamed_addr constant [5 x i8] c"%lld\00", align 1

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #0

; Function Attrs: nofree nounwind
define noundef i32 @main() local_unnamed_addr #0 {
entry:
  %0 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @fmt_str, ptr nonnull @str)
  %putchar = tail call i32 @putchar(i32 10)
  %1 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @fmt_int, i64 0)
  %putchar7 = tail call i32 @putchar(i32 10)
  %2 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @fmt_int, i64 1)
  %putchar7.1 = tail call i32 @putchar(i32 10)
  %3 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @fmt_int, i64 2)
  %putchar7.2 = tail call i32 @putchar(i32 10)
  %4 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @fmt_int, i64 3)
  %putchar7.3 = tail call i32 @putchar(i32 10)
  %5 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @fmt_int, i64 4)
  %putchar7.4 = tail call i32 @putchar(i32 10)
  ret i32 0
}

; Function Attrs: nofree nounwind
declare noundef i32 @putchar(i32 noundef) local_unnamed_addr #0

attributes #0 = { nofree nounwind }
