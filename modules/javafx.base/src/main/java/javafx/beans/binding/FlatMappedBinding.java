package javafx.beans.binding;

import java.util.Objects;
import java.util.function.Function;

import javafx.beans.value.ObservableValue;

class FlatMappedBinding<S, T> extends LazyObjectBinding<T> {
    private final ObservableValue<S> source;
    private final Function<? super S, ? extends ObservableValue<? extends T>> mapper;

    private Subscription mappedSubscription = Subscription.EMPTY;

    public FlatMappedBinding(ObservableValue<S> source, Function<? super S, ? extends ObservableValue<? extends T>> mapper) {
        this.source = Objects.requireNonNull(source, "the observable musn't be null");
        this.mapper = Objects.requireNonNull(mapper, "the mapping function musn't be null");
    }

    @Override
    protected T computeValue() {
        S value = source.getValue();
        ObservableValue<? extends T> mapped = value == null ? null : mapper.apply(value);

        if (isObserved()) {
            mappedSubscription.unsubscribe();
            mappedSubscription = mapped == null ? Subscription.EMPTY : Subscription.subscribeInvalidations(mapped, this::invalidate);
        }

        return mapped == null ? null : mapped.getValue();
    }

    @Override
    protected Subscription observeInputs() {
        Subscription subscription = Subscription.subscribeInvalidations(source, this::invalidate);

        return () -> {
            subscription.unsubscribe();
            mappedSubscription.unsubscribe();
            mappedSubscription = Subscription.EMPTY;
        };
    }
}
