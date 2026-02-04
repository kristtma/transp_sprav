#include <iostream>
#include "transport_catalogue.h"
#include "json_reader.h"

using namespace std;
using namespace transport_catalogue;

int main()
{
    // 1. Создаем транспортный каталог
    transport_catalogue::TransportCatalogue catalogue;
    // 2. Создаем JSON-ридер, передаем ему каталог
    json_reader::JsonReader reader(catalogue);
    // 3. Загружаем данные из std::cin (куда перенаправлен input.json)
    reader.LoadData(std::cin);
    //reader.ProcessRequests(std::cout);
    reader.ProcessRequests(std::cout);
    return 0;
}
