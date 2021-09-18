package javafx.beans.binding;

import java.util.Objects;
import java.util.function.Supplier;

import javafx.beans.value.ObservableValue;

class OrElseGetBinding<T> extends LazyObjectBinding<T> {

    private final ObservableValue<T> source;
    private final Supplier<? extends T> supplier;

    public OrElseGetBinding(ObservableValue<T> source, Supplier<? extends T> supplier) {
        this.source = Objects.requireNonNull(source);
        this.supplier = Objects.requireNonNull(supplier);
    }

    @Override
    protected Subscription observeInputs() {
        // add subscription model
        return null;
    }

    @Override
    protected T computeValue() {
      T value = source.getValue();
      return value == null ? null : supplier.get();
    }
}