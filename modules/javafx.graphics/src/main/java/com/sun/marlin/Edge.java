package com.sun.marlin;

import java.lang.foreign.Arena;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemoryLayout.PathElement;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.lang.invoke.VarHandle;

sealed abstract class Edge {
    
    abstract int getCurxOr();
    abstract int getError();
    abstract int getBumpX();
    abstract int getBumpErr();
    abstract int getNext();
    abstract int getYMax();

    abstract void setCurxOr(int curxOr);
    abstract void setError(int error);
    abstract void setBumpX(int bumpX);
    abstract void setBumpErr(int bumpErr);
    abstract void setNext(int next);
    abstract void setYMax(int yMax);

    static Edge createOnHeap() {
        return new JavaEdge();
    }

    static Edge createOffHeap() {
        return new NativeEdge();
    }
    
    private final static class JavaEdge extends Edge {

        private int curxOr, error, bumpX, bumpErr, next, yMax;
        
        @Override
        int getCurxOr() {
            return curxOr;
        }

        @Override
        int getError() {
            return error;
        }

        @Override
        int getBumpX() {
            return bumpX;
        }

        @Override
        int getBumpErr() {
            return bumpErr;
        }

        @Override
        int getNext() {
            return next;
        }

        @Override
        int getYMax() {
            return yMax;
        }

        @Override
        void setCurxOr(int curxOr) {
            this.curxOr = curxOr;
        }

        @Override
        void setError(int error) {
            this.error = error;
        }

        @Override
        void setBumpX(int bumpX) {
            this.bumpX = bumpX;
        }

        @Override
        void setBumpErr(int bumpErr) {
            this.bumpErr = bumpErr;
        }

        @Override
        void setNext(int next) {
            this.next = next;
        }

        @Override
        void setYMax(int yMax) {
            this.yMax = yMax;
        }        
    }
    
    private final static class NativeEdge extends Edge {

        private static final Arena ARENA = Arena.global();
        
        private static final MemoryLayout EDGE_LAYOUT = MemoryLayout.structLayout(
                ValueLayout.JAVA_INT.withName("curxOr"),
                ValueLayout.JAVA_INT.withName("error"),
                ValueLayout.JAVA_INT.withName("bumpX"),
                ValueLayout.JAVA_INT.withName("bumpErr"),
                ValueLayout.JAVA_INT.withName("next"),
                ValueLayout.JAVA_INT.withName("yMax")
        );

        private static final VarHandle curxOr = EDGE_LAYOUT.varHandle(PathElement.groupElement("curxOr"));
        private static final VarHandle error = EDGE_LAYOUT.varHandle(PathElement.groupElement("error"));
        private static final VarHandle bumpX = EDGE_LAYOUT.varHandle(PathElement.groupElement("bumpX"));
        private static final VarHandle bumpErr = EDGE_LAYOUT.varHandle(PathElement.groupElement("bumpErr"));
        private static final VarHandle next = EDGE_LAYOUT.varHandle(PathElement.groupElement("next"));
        private static final VarHandle yMax = EDGE_LAYOUT.varHandle(PathElement.groupElement("yMax"));

        // size of one edge in bytes
        private static final long SIZEOF_EDGE_BYTES = EDGE_LAYOUT.byteSize();
        
        private final MemorySegment memSeg;
        
        private NativeEdge() {
            memSeg = ARENA.allocate(EDGE_LAYOUT);
        }

        @Override
        void setCurxOr(int curxOr) {
            NativeEdge.curxOr.set(memSeg, 0, curxOr);
        }

        @Override
        void setError(int error) {
            NativeEdge.error.set(memSeg, 0, error);
        }

        @Override
        void setBumpX(int bumpX) {
            NativeEdge.bumpX.set(memSeg, 0, bumpX);
        }

        @Override
        void setBumpErr(int bumpErr) {
            NativeEdge.bumpErr.set(memSeg, 0, bumpErr);
        }

        @Override
        void setNext(int next) {
            NativeEdge.next.set(memSeg, 0, next);
        }

        @Override
        void setYMax(int yMax) {
            NativeEdge.yMax.set(memSeg, 0, yMax);
        }

        @Override
        int getCurxOr() {
            return (int) NativeEdge.curxOr.get();
        }

        @Override
        int getError() {
            return (int) NativeEdge.error.get();
        }

        @Override
        int getBumpX() {
            return (int) NativeEdge.bumpX.get();
        }

        @Override
        int getBumpErr() {
            return (int) NativeEdge.bumpErr.get();
        }

        @Override
        int getNext() {
            return (int) NativeEdge.next.get();
        }

        @Override
        int getYMax() {
            return (int) NativeEdge.yMax.get();
        }
    }
}