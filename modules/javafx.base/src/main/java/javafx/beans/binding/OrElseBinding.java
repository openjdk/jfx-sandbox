package javafx.beans.binding;

import java.util.Objects;

import javafx.beans.value.ObservableValue;

class OrElseBinding<T> extends LazyObjectBinding<T> {

    private final ObservableValue<T> source;
    private final T constant;

    public OrElseBinding(ObservableValue<T> source, T constant) {
        this.source = Objects.requireNonNull(source);
        this.constant = constant;
    }

    @Override
    protected Subscription observeInputs() {
        // add subscription model
        return null;
    }

    @Override
    protected T computeValue() {
      T value = source.getValue();
      return value == null ? null : constant;
    }
}