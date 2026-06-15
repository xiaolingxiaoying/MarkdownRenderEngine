#include <iostream>
#include <string>

#include <mwrender/engine.hpp>

int main() {
    const std::string markdown =
        "# Installed API\n\nThis document uses **MWRender**.";

    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;

    const auto result = engine.render(request);
    if (!result.ok) {
        return 1;
    }

    std::cout << result.html;
    return 0;
}
