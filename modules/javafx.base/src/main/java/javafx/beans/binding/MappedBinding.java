package javafx.beans.binding;

import java.util.Objects;
import java.util.function.Function;

import javafx.beans.value.ObservableValue;

class MappedBinding<S, T> extends LazyObjectBinding<T> {

    private final ObservableValue<S> source;
    private final Function<? super S, ? extends T> mapper;

    public MappedBinding(ObservableValue<S> source, Function<? super S, ? extends T> mapper) {
        this.source = Objects.requireNonNull(source);
        this.mapper = Objects.requireNonNull(mapper);
    }

    @Override
    protected Subscription observeInputs() {
        return Subscription.subscribeInvalidations(source, this::invalidate); // start observing source
    }

    @Override
    protected T computeValue() {
      S value = source.getValue();
      return value == null ? null : mapper.apply(value);
    }
}