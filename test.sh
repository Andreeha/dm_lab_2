# valgrind --track-origins=yes -s --leak-check=full
clear
echo "DELETE TABLE"
   ./build/delete_table
echo "CREATE TABLE"
   ./build/create_table
echo "INSERT ID=1"
   ./build/insert <<< 1
echo "INSERT ID=2"
   ./build/insert <<< 2
echo "INSERT ID=-1"
   ./build/insert <<< -1
echo "DELETE ID=2"
   ./build/delete <<< 2
echo "DELETE birth=\"Y2024 M12 D14 h11 m11 s11 ms123\""
   ./build/delete_many
echo "FIND birth=\"Y2024 M12 D14 h11 m11 s11 ms123\""
   ./build/find_many
echo "INSERT ID=3"
   ./build/insert <<< 3
echo "INSERT ID=4"
   ./build/insert <<< 4
echo "INSERT ID=4"
   ./build/insert <<< 4
echo "INSERT ID=5"
   ./build/insert <<< 5
echo "FIND birth=\"Y2024 M12 D14 h11 m11 s11 ms123\""
   ./build/find_many
echo "FIND id = 0"
   ./build/find <<< 0
echo "FIND id = 4"
   ./build/find <<< 4
echo "BACKUP"
   ./build/backup
echo "ERASE TABLE"
   ./build/erase_table
