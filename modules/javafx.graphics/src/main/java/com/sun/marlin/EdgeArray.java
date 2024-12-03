package com.sun.marlin;

import java.lang.foreign.MemorySegment;
import java.util.ArrayList;
import java.util.List;

sealed abstract class EdgeArray {

    static EdgeArray edges;

    static EdgeArray allocate(int initialCount) {
        edges = new JavaEdgeArray(initialCount);
        return edges;
    }
    
    abstract void add(Edge edge);
    abstract void remove(Edge edge);
    abstract void resize(int size);
    abstract void clear();
    
    private final static class JavaEdgeArray extends EdgeArray {

        private final ArrayList<Edge> edges;
        
        JavaEdgeArray(int initialCount) {
            edges = new ArrayList<>(initialCount);
        }
        
        @Override
        void add(Edge edge) {
            edges.add(edge);
        }

        @Override
        void remove(Edge edge) {
            edges.remove(edge);
        }

        @Override
        void resize(int size) {
            edges.ensureCapacity(size);
        }

        @Override
        void clear() {
            edges.clear();
        }
    }

    private final static class NativeEdgeArray extends EdgeArray {

        MemorySegment edges;
    }
}