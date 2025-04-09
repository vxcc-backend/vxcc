use vxcc_sys::*;
use std::ptr::{null, null_mut};

unsafe extern "C" {
    static stdout: *mut libc::FILE;
}

fn main() {
    let block = unsafe { vx_IrBlock_initHeap(null_mut(), null_mut()) };
    unsafe { vx_IrBlock_dump(block, stdout, 0) };
}
