// a api to transform callback to coroutine
#include <coroutine>
#include <optional>
co_await callBackTransform([](std h) {
  f([] {
    // 存入返回值，然后resume
    h.resumeWith(std::optional<Result>);
  })
})

    // 链式调用
    // 所有的操作都是由一个flow返回另一个flow
    // Flow<A> -> Flow<B>
    void create() {
  flow::create(Dispatcher::IO, []() {
    const res = await get("'x");
    co_await emit("aa");
    co_await emit("aaa");
  })
  >> flowOn(Dispatcher.IO))
  >>  mergeWith()
  >> transform()
  >> collect([]() {

  })
  >>
}