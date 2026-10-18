use bt_data::seedpack::{NSpec, PackEntry, load_pack};
use std::alloc::{GlobalAlloc, Layout, System};
use std::backtrace::Backtrace;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Mutex, OnceLock};

static BIG: AtomicU64 = AtomicU64::new(0);
static LOCK: OnceLock<Mutex<()>> = OnceLock::new();

struct Tracing;

unsafe impl GlobalAlloc for Tracing {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        if layout.size() > 1 << 30 {
            let n = BIG.fetch_add(1, Ordering::SeqCst);
            if n == 0 && LOCK.get_or_init(|| Mutex::new(())).lock().is_ok() {
                eprintln!("big alloc {} bytes align {}", layout.size(), layout.align());
                eprintln!("{}", Backtrace::force_capture());
            }
        }
        unsafe { System.alloc(layout) }
    }

    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        unsafe { System.dealloc(ptr, layout) }
    }

    unsafe fn alloc_zeroed(&self, layout: Layout) -> *mut u8 {
        if layout.size() > 1 << 30 {
            let n = BIG.fetch_add(1, Ordering::SeqCst);
            if n == 0 && LOCK.get_or_init(|| Mutex::new(())).lock().is_ok() {
                eprintln!(
                    "big alloc_zeroed {} bytes align {}",
                    layout.size(),
                    layout.align()
                );
                eprintln!("{}", Backtrace::force_capture());
            }
        }
        unsafe { System.alloc_zeroed(layout) }
    }

    unsafe fn realloc(&self, ptr: *mut u8, layout: Layout, new_size: usize) -> *mut u8 {
        if new_size > 1 << 30 {
            let n = BIG.fetch_add(1, Ordering::SeqCst);
            if n == 0 && LOCK.get_or_init(|| Mutex::new(())).lock().is_ok() {
                eprintln!("big realloc to {new_size} bytes");
                eprintln!("{}", Backtrace::force_capture());
            }
        }
        unsafe { System.realloc(ptr, layout, new_size) }
    }
}

#[global_allocator]
static A: Tracing = Tracing;

fn main() {
    let entry = PackEntry {
        file: "pyraminx_l4e.tsv".to_string(),
        event: "pyraminx".to_string(),
        method: "l4e".to_string(),
        stage: "l4e".to_string(),
        n: NSpec::Unset,
        expected_rows: 35,
    };
    eprintln!("before load");
    let rows = load_pack("../../ref_ground/packs".as_ref(), &entry).expect("pack loads");
    eprintln!("after load: {} rows", rows.len());
}
