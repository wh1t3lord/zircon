// test_classes.hpp
#pragma once
#include <memory>
#include <vector>
#include <concepts>
#include <type_traits>

namespace kotek
{
    class A {};
    namespace ktk
    {
        class B{};
        namespace groid
        {
            class C{};
        }
    }
}

namespace UnitTest::ComplexClasses {
    // Base template with CRTP
    template<typename T>
    class AbstractBase {
    public:
        virtual ~AbstractBase() = default;
        virtual void abstract_method() const = 0;
        int base_data = 42;
    };

    // Complex inheritance hierarchy
    template<typename U, typename V>
    class TemplatedDerived : public virtual AbstractBase<U>,
                             private std::enable_shared_from_this<V> {
    protected:
        U template_data;
        mutable V volatile_data;
        
        struct Nested {
            double nested_value;
            decltype(auto) nested_method() const {
                return volatile_data;
            }
        };
        
    public:
        [[nodiscard]] constexpr auto get_data() const noexcept -> decltype(auto) {
            return template_data;
        }
        
        void abstract_method() const override final {
            // Implementation
        }
    };

    // Concept-constrained template
    template<std::floating_point T>
    class SpecializedPhysicsEntity : public AbstractBase<T> {
        alignas(64) T aligned_data;
        mutable std::atomic<T> atomic_value;
        static inline constexpr double GRAVITY = 9.81;
        
        friend class PhysicsSystem;
        
    public:
        using value_type = T;
        
        template<typename... Args>
        requires(std::constructible_from<T, Args> && ...)
        SpecializedPhysicsEntity(Args&&... args) 
            : aligned_data{std::forward<Args>(args)...} {}
            
        T calculate_force() const {
            return aligned_data * GRAVITY;
        }
    };

    // Variadic template with fold expressions
    template<typename... Ts>
    class CompositeEntity : private Ts... {
        [[no_unique_address]] std::tuple<Ts...> components;
        std::variant<Ts...> active_component;
        
        static constinit inline size_t instance_count = 0;
        
    public:
        CompositeEntity() { ++instance_count; }
        
        template<typename T>
        requires(std::disjunction_v<std::is_same<T, Ts>...>)
        auto& get_component() {
            return std::get<T>(components);
        }
    };

    // Perfect forwarding and SFINAE
    class AdvancedResource {
        std::unique_ptr<int[]> resource;
        std::vector<std::function<void()>> callbacks;
        
        template<typename F>
        using is_callback = std::is_invocable_r<void, F>;
        
    public:
        template<typename F, typename = std::enable_if_t<is_callback<F>::value>>
        void register_callback(F&& f) {
            callbacks.emplace_back(std::forward<F>(f));
        }
        
        decltype(auto) operator->() const {
            return resource.get();
        }
    };

    // Template specialization with attributes
    template<>
    class SpecializedPhysicsEntity<double> : public AbstractBase<double> {
        [[deprecated("Use float specialization instead")]] 
        double deprecated_value;
        
        union {
            double precise_data;
            float approximate_data;
        };
        
    public:
        using AbstractBase::base_data;
        
        consteval SpecializedPhysicsEntity() noexcept = default;
        
        explicit(false) operator float() const {
            return static_cast<float>(precise_data);
        }
    };

    // CRTP with parameter pack
    template<template<typename...> typename Base, typename... Args>
    class MACRO_1111 MetaFactory : public Base<Args...> {
        using BaseType = Base<Args...>;
        typename BaseType::value_type factory_data;
        
        template<typename T>
        struct Rebind {
            using type = Base<T>;
        };
        
    public:
        using Base<Args...>::Base;
        
        template<typename T>
        auto rebind() -> Rebind<T>::type {
            return {factory_data};
        }
    };

    // Complex nested type with attributes
    class [[nodiscard]] FinalEntity final :
        public MetaFactory<CompositeEntity, AdvancedResource, SpecializedPhysicsEntity<float>> {
        inline static thread_local size_t thread_local_counter = 0;
        mutable std::recursive_mutex mutex;
        std::conditional_t<sizeof(void*) == 8, uint64_t, uint32_t> platform_data;
        
        enum class InternalState : uint8_t {
            INITIALIZING,
            RUNNING,
            TERMINATING
        } state;
        
    public:
        FinalEntity() = delete;
        explicit FinalEntity(auto&&... args)
            requires(std::constructible_from<AdvancedResource, decltype(args)...>)
            : MetaFactory(std::forward<decltype(args)>(args)...) {}
            
        FinalEntity(FinalEntity&&) noexcept = default;
        
        ~FinalEntity() {
            std::lock_guard lock(mutex);
            // Cleanup logic
        }
    };
} // namespace UnitTest::ComplexClasses